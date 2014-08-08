#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        if(pmd->porpd != 0) {
            pmd->porlen = n;
        } else {
            if(pmd->len != n) {
                pmd->len = n;
                tick2beat(pmd->len, beats);
                printf("l%s", beats);
            }
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

