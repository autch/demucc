#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define LogL(format, ...) \
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

void get_note(int note)
{
	// o4c = 60
	char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
	int k = note % 12, oct = (note - 12) / 12;

	printf("o%d%s", oct, key[k]);
}

int main(int ac, char** av)
{
	FILE* fp = NULL;
	struct pmd pmd;
	char* filename = NULL;
	uint8_t* p;

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

	{
		int s = 1, i;

		for(i = 0; i < pmd.num_parts; i++, s += 2) {
			uint16_t addr = read_u16(&p);
			pmd.parts[i] = pmd.buffer + addr;
		}
	}

	pmd.drums = pmd.buffer + read_u16(&p);
	pmd.title = (char*)(pmd.buffer + read_u16(&p));
	pmd.title2 = (char*)(pmd.buffer + read_u16(&p));

	printf("#Title %s\n", pmd.title);
	printf("#Title2 %s\n", pmd.title2);

	{
		uint16_t stack[STACK_SIZE];
		int i;
		for(i = 0; i < pmd.num_parts; i++) {
			uint8_t* p = pmd.parts[i];
			char partname = 'A' + i;
			int len, drum_track;
			int sp = 0;

			printf("%c ", partname);
			
			drum_track = 0;

			for(;;) {
				uint8_t n = read_u8(&p);

				if(n >= 0xe0) {
					int cmd = n - 0xe0;

					switch(cmd) {
					case 0x00:
						// rest
						printf("r");
						break;
					case 0x01:
					{
						// gate
						int gate = read_u8(&p);
						printf("q%d", -(24 * gate / 99 - 24));
						break;
					}
					case 0x02:
					{
						// jump
						uint16_t addr = read_u16(&p);
						// FIXME: MMLコマンドとしてある？
						p = pmd.buffer + addr;
						break;
					}
					case 0x03:
					{
						// call
						uint16_t addr = read_u16(&p);
						uint8_t times = read_u8(&p);
						uint16_t here = p - pmd.buffer;
						
						stack[sp++] = times;
						stack[sp++] = here;
						stack[sp++] = addr;
						
						p = pmd.buffer + addr;

						break;
					}
					case 0x04:
						// repeat
						printf("[%d", read_u8(&p)); // FIXME: ループ回数は next に書かないといけない
						break;
					case 0x05:
						// next
						printf("]");
						break;
					case 0x06:
						// transpose
						printf("_%d", read_u8(&p)); // FIXME: 単位がおかしい
						break;
					case 0x07:
						// tempo
						printf("T%d", read_u8(&p));
						break;
					case 0x08:
						// inst
						printf("@%d", read_u8(&p));
						break;
					case 0x09:
						// volume
						printf("V%d", read_u8(&p));
						break;
					case 0x0a:
					{
						// envelope
						int ar, dr, sl, sr;
						ar = read_u8(&p);
						dr = read_u8(&p);
						sl = read_u8(&p);
						sr = read_u8(&p);
						printf("E%d,%d,%d,%d", ar, dr, sl, sr);
						break;
					}
					case 0x0b:
						// detune
						printf("D%d", read_u8(&p)); // FIXME: 単位が変
						break;
					case 0x0c:
					{
						// note 2
						int note;

						note = read_u8(&p);
						if(note >= 0x80) note -= 0x80 - 12;
						
						get_note(note);
						break;
					}
					case 0x0d:
						// portamento params
						printf("P%d,%d", read_u8(&p), read_u8(&p));
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
						printf("RT%d", read_u8(&p));
						drum_track = 1;
						break;
					case 0x11:
					{
						// vibrato
						int depth, speed, delay, rate;
						depth = read_u8(&p);
						speed = read_u8(&p);
						delay = read_u8(&p);
						rate = read_u8(&p);
						printf("Y%d,%d,%d,%d", depth, speed, delay, rate);
						break;
					}
					case 0x12:
						// master volume
						read_u8(&p);
						break;
					case 0x13:
						// master fade
						read_u16(&p);
						break;
					case 0x14:
						// part fade
						read_u16(&p);
						break;
					case 0x15:
						// bend
						read_u16(&p);
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
						printf("v%d", read_u8(&p));
						break;
					case 0x1b:
					{
						int8_t vol = read_u8(&p);
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
				} else if(n >= 0x80) {
					if(drum_track == 1) {
						int index = n - 0x80;
						uint8_t* pp = pmd.drums + (index << 1);
						uint16_t addr = read_u16(&pp);

						stack[sp++] = 1;
						stack[sp++] = p - pmd.buffer;
						stack[sp++] = addr;
						p = pmd.buffer + addr;
					} else {
						int note = n - 0x80 + 12;
						get_note(note);
					}					
				} else if(n != 0) {
					if(n == 127) {
						n = read_u8(&p);
						if(n < 127) n += 256;
					}
					len = n;
					printf("l%d", (4 * TPQN) / len);
				} else {
					// n == 0
					int lsp = sp;
					if(lsp == 0) {
						break;
					}

					lsp -= 3;
					if(--stack[lsp] != 0) {
						p = pmd.buffer + stack[lsp + 2];
					} else {
						p = pmd.buffer + stack[lsp + 1];
						sp = lsp;
					}
				}
			}

			printf("\n\n; ====\n\n");
		}
	}

	free(pmd.parts);
	free(pmd.buffer);

	return 0;
}


