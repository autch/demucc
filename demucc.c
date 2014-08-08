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

    pmd.mmlbuf = mmlbuf_new();

	extract_drums(&pmd);

	for(i = 0; i < pmd.num_parts; i++) {
		uint8_t* p = pmd.parts[i];
		char partname = 'A' + i;
        int return_addr;

		if(pmd.parts[i] == NULL)
			continue;
       
        /* pass 1 */
        reset_part_ctx(&pmd);
        pmd.part = i;
        mmlbuf_reset(pmd.mmlbuf);
        read_notes(&pmd, &p);
        // save return addr
        return_addr = pmd.return_addr;

        /* pass 2 */
        p = pmd.parts[i];
        reset_part_ctx(&pmd);
        pmd.part = i;
        pmd.return_addr = return_addr;
        mmlbuf_reset(pmd.mmlbuf);

        mml_printf(&pmd, "%c ", partname);
        read_notes(&pmd, &p);
		printf("%s\n\n", mmlbuf_buf(pmd.mmlbuf));
	}

    mmlbuf_free(pmd.mmlbuf);
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

        reset_part_ctx(pmd);
        pmd->drum_track = 1;
        pmd->sp = 0;
        pmd->len = 0;
        pmd->part = -1;
        mmlbuf_reset(pmd->mmlbuf);

		mml_printf(pmd, "!!%sg ", drumname);
	
		p = pmd->buffer + *pd;
		read_notes(pmd, &p);

        printf("%s\n", mmlbuf_buf(pmd->mmlbuf));

		pd++;
		i++;
	}

	printf("\n");

	return 0;
}


