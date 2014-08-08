#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LogL(format, ...)												\
	fprintf(stderr, ("%s:%s(%d): " format "\n"), __FILE__, __func__, __LINE__, ##__VA_ARGS__)

#define TPQN 24
#define STACK_SIZE 32 // muslib は 24
#define DRUMNAME_MAX 16

struct pmd {
	/* global context */
	uint8_t* buffer;
	long buffer_size;
	uint8_t* p;
	uint8_t** parts;
	uint8_t* drums;
	int num_parts;
	char* title;
	char* title2;

	/* current part context */
	int stack[STACK_SIZE];
	int sp;
	int len;
	int drum_track;
	int legato;
    int oct;
};

uint16_t read_u16(uint8_t** pp)
{
	uint16_t t;
	uint8_t* p = *pp;

	t = p[1] << 8 | p[0];
	*pp += 2;
	return t;
}

uint8_t read_u8(uint8_t** pp)
{
	uint8_t t;
	uint8_t* p = *pp;

	t = *p;
	*pp += 1;
	return t;
}

// BUG: 連符と付点がつけられない
int tick2beat(int tick, char* notes)
{
    char* p = notes;
    int curlen = TPQN * 4;
    int meas = 1;

/*
  if(tick % 3 == 0
  || tick % 5 == 0
  || tick % 7 == 0) {
  sprintf(p, "%%%d", tick);
  return 0;
  }
*/
    while(tick != 0) {
        if(curlen <= tick) {
            tick -= curlen;
            p += sprintf(p, "%d", meas);
            break;
        }
        curlen /= 2;
        meas *= 2;
    }
    while(tick > 0) {
        if(curlen <= tick) {
            tick -= curlen;
            p += sprintf(p, ".");
        }
        curlen /= 2;
        meas *= 2;
    }
    // FIXME: n連符ってこれで出る？

    return 0;
}

void get_note(struct pmd* pmd, int note, int tick)
{
	// o4c = 60
	char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
	int k = note % 12, oct = (note - 12) / 12;
    int oct_diff;
    char beats[16];

	if(tick > 0) {
		// FIXME: 連符で割り切れないときは直接 %tick で指定する
        if(note > 0) {
            oct_diff = oct - pmd->oct;
            pmd->oct = oct;
            switch(oct_diff) {
            case 0:
                break;
            case 1:
                printf(">");
                break;
            case -1:
                printf("<");
                break;
            default:
                printf("o%d", oct);
            }
        }
        tick2beat(tick, beats);
		printf("%s%s", (note == -1) ? "r" : key[k], pmd->legato ? "&" : "");
	} else {
		// for use in drums
		printf("?o%d%s%s", oct, key[k], pmd->legato ? "&" : "");
	}
}

// ドラムシーケンス番号から適当にドラムパート名をでっちあげる
int get_drumname(int note, char* buffer, size_t size)
{
	char* p = buffer;
	char* pe = buffer + size;

	while(p != pe) {
		*p++ = 'a' + (note % 26);
		note /= 26;
		if(note == 0) break;
	}

	*p++ = '\0';

	return p - buffer;
}

void reset_part_ctx(struct pmd* pmd)
{
    memset(pmd->stack, 0, sizeof pmd->stack);
    pmd->drum_track = 0;
    pmd->sp = 0;
    pmd->len = 4;
    pmd->oct = -1;
}

int read_notes(struct pmd* pmd, uint8_t** pp);
int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp);
int extract_drums(struct pmd* pmd);

int main(int ac, char** av)
{
	FILE* fp = NULL;
	struct pmd pmd;
	char* filename = NULL;
	uint8_t* p;
	int i;

	memset(&pmd, 0, sizeof pmd);

	if(ac != 2) {
		printf("Usage: %s filename.pmd\n", *av);
		return -1;
	}

	filename = *++av;

	if((fp = fopen(filename, "rb")) == NULL) {
		perror(filename);
		return 1;
	} 

	fseek(fp, 0, SEEK_END);
	pmd.buffer_size = ftell(fp);
	rewind(fp);

	pmd.buffer = p = malloc(pmd.buffer_size);
	fread(pmd.buffer, 1, pmd.buffer_size, fp);
	fclose(fp);

	if((pmd.num_parts = read_u8(&p)) == 0) {
		pmd.num_parts = read_u8(&p);
	}

	pmd.parts = calloc(pmd.num_parts, sizeof(uint8_t*));

	for(i = 0; i < pmd.num_parts; i++) {
		uint16_t addr = read_u16(&p);
		if(addr != 0) {
			pmd.parts[i] = pmd.buffer + addr;
		}
	}

	pmd.drums = pmd.buffer + read_u16(&p);
	pmd.title = (char*)(pmd.buffer + read_u16(&p));
	pmd.title2 = (char*)(pmd.buffer + read_u16(&p));

	if(pmd.title != (char*)pmd.buffer) {
		printf("#Title %s\n", pmd.title);
	}
	if(pmd.title2 != (char*)pmd.buffer) {
		printf("#Title2 %s\n\n", pmd.title2);
	}

	extract_drums(&pmd);

	for(i = 0; i < pmd.num_parts; i++) {
		uint8_t** pp = pmd.parts + i;
		char partname = 'A' + i;

		if(pmd.parts[i] == NULL)
			continue;

        reset_part_ctx(&pmd);

		printf("%c ", partname);
			
		while(read_notes(&pmd, pp) == 0) {
			//
		}
		printf("\n\n");
	}

	free(pmd.parts);
	free(pmd.buffer);

	return 0;
}

int extract_drums(struct pmd* pmd)
{
	uint16_t* pd = (uint16_t*)pmd->drums;
	uint8_t* p;
	int i = 0;
	char drumname[DRUMNAME_MAX];

	if(pmd->drums == pmd->buffer) {
		return 0;
	}

	while(1) {
		if((uint8_t*)pd >= pmd->buffer + pmd->buffer_size)
			break;

		get_drumname(i, drumname, sizeof drumname);
		printf("!!%sg ", drumname);

        reset_part_ctx(pmd);
        pmd->drum_track = 1;
        pmd->sp = 0;
        pmd->len = 0;
	
		p = pmd->buffer + *pd;
		while(read_notes(pmd, &p) == 0) {
			//
		}

		printf("\n");

		pd++;
		i++;
	}

	printf("\n");

	return 0;
}

/**
 * @return パート終了のとき 1, そうでないとき 0
 */
int read_notes(struct pmd* pmd, uint8_t** pp)
{
	uint8_t n = read_u8(pp);

	if(n >= 0xe0) {
		if(read_commands(pmd, n, pp) != 0) {
			return 1;
		}
	} else if(n >= 0x80) {
		if(pmd->drum_track == 1) {
			int index = n - 0x80;
			char drumname[DRUMNAME_MAX];
            char beats[16];

			get_drumname(index, drumname, sizeof drumname);
            tick2beat(pmd->len, beats);
			printf("!%sg ", drumname);
		} else {
			int note = n - 0x80 + 12;
			get_note(pmd, note, pmd->len);
		}					
	} else if(n != 0) {
        char beats[16];
		if(n == 127) {
			n = read_u8(pp);
			if(n < 127) n += 256;
		}
        if(pmd->len != n) {
            pmd->len = n;
            tick2beat(pmd->len, beats);
            printf("l%s", beats);
        }
	} else {
		// n == 0
		int lsp = pmd->sp;
		if(lsp == 0) {
			return 1;
		}
		
		lsp -= 3;
		if(--pmd->stack[lsp] != 0) {
            // repeat
			*pp = pmd->buffer + pmd->stack[lsp + 2];
		} else {
            // return
			*pp = pmd->buffer + pmd->stack[lsp + 1];
			pmd->sp = lsp;
		}
	}
	return 0;
}

int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp)
{
	int cmd = n - 0xe0;

	switch(cmd) {
	case 0x00:
		// rest
        get_note(pmd, -1, pmd->len);
		break;
	case 0x01:
	{
		// gate
		int gate = read_u8(pp);
		printf("q%d", -(24 * gate / 99 - 24));
		break;
	}
	case 0x02:
	{
		// jump
		uint16_t addr = read_u16(pp);
		// FIXME: 飛び先に L を挿入しなければならない
		// *pp = pmd->buffer + addr;
		//printf("L");
		return 1;
		break;
	}
	case 0x03:
	{
		// call
		uint16_t addr = read_u16(pp);
		uint8_t times = read_u8(pp);
		uint16_t here = *pp - pmd->buffer;
						
		pmd->stack[pmd->sp++] = times;
		pmd->stack[pmd->sp++] = here;
		pmd->stack[pmd->sp++] = addr;
						
		*pp = pmd->buffer + addr;
		break;
	}
	case 0x04:
	{
		// repeat
		uint8_t times = read_u8(pp);
		uint16_t here = *pp - pmd->buffer;

		pmd->stack[pmd->sp++] = times;
		pmd->stack[pmd->sp++] = here;
		pmd->sp++;
		// next のときにループ回数を覚えておくだけなので、ジャンプはしない

		printf("[");
		break;
	}
	case 0x05:
	{
		// next
		int sp = pmd->sp - 3;
		uint8_t times = pmd->stack[sp];
		pmd->sp = sp;
		// スタックはループ回数を覚えるためだけに使ったので、ジャンプはしない

		printf("]%d", times);
		break;
	}
	case 0x06:
		// transpose
		printf("_%d", (int8_t)read_u8(pp)); // FIXME: 単位がおかしい
		break;
	case 0x07:
		// tempo
		printf("T%d", read_u8(pp));
		break;
	case 0x08:
		// inst
		printf("@%d", read_u8(pp));
		break;
	case 0x09:
		// volume
		printf("V%d", read_u8(pp));
		break;
	case 0x0a:
	{
		// envelope
		int ar, dr, sl, sr;
		ar = read_u8(pp);
		dr = read_u8(pp);
		sl = read_u8(pp);
		sr = read_u8(pp);
		printf("E%d,%d,%d,%d", ar, dr, sl, sr);
		break;
	}
	case 0x0b:
		// detune
		printf("D%d", (int8_t)read_u8(pp)); // FIXME: 単位が変
		break;
	case 0x0c:
	{
		// note 2
		int note = read_u8(pp);
		if(note >= 0x80) note -= 0x80 - 12;
        get_note(pmd, note, pmd->len);
		break;
	}
	case 0x0d:
		// portamento params
		printf("P%d,%d", read_u8(pp), read_u8(pp)); // FIXME: portamento off のときに長さが必要
		break;
	case 0x0e:
		// portamento on
		printf("{");
		break;
	case 0x0f:
		// portamento off
		printf("}"); // FIXME: 何分音符分かけるかの指定が必要
		break;
	case 0x10:
		// drum track mode
		pmd->drum_track = read_u8(pp);
		printf("=%d", pmd->drum_track); // FIXME: 1 以外は未実装
		break;
	case 0x11:
	{
		// vibrato
		int depth, speed, delay, rate;
		depth = read_u8(pp);
		speed = read_u8(pp);
		delay = read_u8(pp);
		rate = read_u8(pp);
		printf("Y%d,%d,%d,%d", depth, speed, delay, rate);
		break;
	}
	case 0x12:
		// master volume
		read_u8(pp);
		break;
	case 0x13:
		// master fade
		read_u16(pp);
		break;
	case 0x14:
		// part fade
		read_u16(pp);
		break;
	case 0x15:
		// bend
		read_u16(pp);
		break;
	case 0x16:
		// break
		printf(":");
		break;
	case 0x17:
		// nop
		break;
	case 0x18:
		// legato on
		pmd->legato = 1;
		break;
	case 0x19:
		// legato off
		pmd->legato = 0;
		break;
	case 0x1a:
		// expression
		printf("v%d", read_u8(pp));
		break;
	case 0x1b:
	{
		int8_t vol = read_u8(pp);
		// rel. expression
		if(vol >= 0) {
			printf("v+%d", vol);
		} else {
			printf("v%d", vol);
		}
		break;
	}
	case 0x1c:
	case 0x1d:
	case 0x1e:
	case 0x1f:
	default:
		// nop
		break;
	}

	return 0;
}
