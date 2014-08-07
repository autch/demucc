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
#define STACK_SIZE 256 // muslib は 24

struct pmd {
	uint8_t* buffer;
	long buffer_size;
	uint8_t* p;
	uint8_t** parts;
	uint8_t* drums;
	int num_parts;

	char* title;
	char* title2;

	int stack[STACK_SIZE];
	int sp;

	int len;
	int drum_track;
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

int tick2beat(int tick)
{
	return (4 * TPQN) / tick;
}

void get_note(int note, int tick)
{
	// o4c = 60
	char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
	int k = note % 12, oct = (note - 12) / 12;

	printf("o%d%s%d", oct, key[k], tick2beat(tick));
}

int read_notes(struct pmd* pmd, uint8_t** pp);
int read_commands(struct pmd* pmd, uint8_t n, uint8_t** pp);

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

	LogL("%d parts", pmd.num_parts);

	pmd.parts = calloc(pmd.num_parts, sizeof(uint8_t*));

	for(i = 0; i < pmd.num_parts; i++) {
		uint16_t addr = read_u16(&p);
		pmd.parts[i] = pmd.buffer + addr;
	}

	pmd.drums = pmd.buffer + read_u16(&p);
	pmd.title = (char*)(pmd.buffer + read_u16(&p));
	pmd.title2 = (char*)(pmd.buffer + read_u16(&p));

	printf("#Title %s\n", pmd.title);
	printf("#Title2 %s\n\n", pmd.title2);

	for(i = 0; i < pmd.num_parts; i++) {
		uint8_t** pp = pmd.parts + i;
		char partname = 'A' + i;

		pmd.drum_track = 0;
		pmd.sp = 0;
		pmd.len = 0;

		printf("%c ", partname);
			
		while(!read_notes(&pmd, pp)) {
			//
		}

		printf("\n\n; ====\n\n");
	}

	free(pmd.parts);
	free(pmd.buffer);

	return 0;
}

/**
 * @return パート終了のとき 1, そうでないとき 0
 */
int read_notes(struct pmd* pmd, uint8_t** pp)
{
	uint8_t n = read_u8(pp);

	if(n >= 0xe0) {
		read_commands(pmd, n, pp);
	} else if(n >= 0x80) {
		if(pmd->drum_track == 1) {
			int index = n - 0x80;
			uint8_t* pd = pmd->drums + (index << 1);
			uint16_t addr = read_u16(&pd);
			
			pmd->stack[pmd->sp++] = 1;
			pmd->stack[pmd->sp++] = *pp - pmd->buffer;
			pmd->stack[pmd->sp++] = addr;
			*pp = pmd->buffer + addr;
			
			printf(" ");
		} else {
			int note = n - 0x80 + 12;
			get_note(note, pmd->len);
		}					
	} else if(n != 0) {
		if(n == 127) {
			n = read_u8(pp);
			if(n < 127) n += 256;
		}
		pmd->len = n;
		// printf("l%d", tick2beat(pmd.len));
	} else {
		// n == 0
		int lsp = pmd->sp;
		if(lsp == 0) {
			return 1;
		}
		
		lsp -= 3;
		if(--pmd->stack[lsp] != 0) {
			*pp = pmd->buffer + pmd->stack[lsp + 2];
		} else {
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
		printf("r%d", tick2beat(pmd->len));
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
		// FIXME: MMLコマンドとしてある？
		*pp = pmd->buffer + addr;
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
		// repeat
		printf("[%d", read_u8(pp)); // FIXME: ループ回数は next に書かないといけない
		break;
	case 0x05:
		// next
		printf("]");
		break;
	case 0x06:
		// transpose
		printf("_%d", read_u8(pp)); // FIXME: 単位がおかしい
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
		printf("D%d", read_u8(pp)); // FIXME: 単位が変
		break;
	case 0x0c:
	{
		// note 2
		int note;

		note = read_u8(pp);
		if(note >= 0x80) note -= 0x80 - 12;
						
		get_note(note, pmd->len);
		break;
	}
	case 0x0d:
		// portamento params
		printf("P%d,%d", read_u8(pp), read_u8(pp));
		break;
	case 0x0e:
		// portamento on
		printf("{");
		break;
	case 0x0f:
		// portamento off
		printf("}");
		break;
	case 0x10:
		// drum track mode
		printf("RT%d", read_u8(pp));
		pmd->drum_track = 1;
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
		printf("&"); // FIXME: muccでコンパイルできない
		break;
	case 0x19:
		// legato off
		printf("!"); // FIXME: どこで使う？
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
