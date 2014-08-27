#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

int demucc(struct pmd* pmd);

#ifndef DEMUCC_NO_MAIN

int usage();

struct option options[] =
{
    { "columns", 1, NULL, 'c' },
    { "ticks", 0, NULL, 't' },
    { "help", 0, NULL, '?' },
    { NULL, 0, NULL, 0 }
};

int main(int ac, char** av)
{
    FILE* fp = NULL;
    struct pmd pmd;
    char* filename = NULL;
    int opt, opt_index = -1;

    memset(&pmd, 0, sizeof pmd);
    pmd.mml_columns = MML_COLUMNS;

    while((opt = getopt_long(ac, av, "c:t", options, &opt_index)) != -1) {
        switch(opt) {
        case 'c':
            pmd.mml_columns = strtol(optarg, NULL, 10);
            break;
        case 't':
            pmd.use_ticks = 1;
            break;
        case '?':
        default:
            return usage();
        }
    }

    if(optind >= ac) {
        return usage();
    }

    filename = av[optind];

    if((fp = fopen(filename, "rb")) == NULL) {
        perror(filename);
        return 1;
    } 

    fseek(fp, 0, SEEK_END);
    pmd.buffer_size = ftell(fp);
    rewind(fp);

    pmd.buffer = malloc(pmd.buffer_size);
    fread(pmd.buffer, 1, pmd.buffer_size, fp);
    fclose(fp);

    demucc(&pmd);

    mmlbuf_free(pmd.mmlbuf);
    free(pmd.parts);
    free(pmd.buffer);

    return 0;
}

int usage()
{
    printf("Usage: demucc [-tc] FILENAME.pmd\n");
    printf("    -c COLUMNS\twrap every COLUMNs (default: %d)\n", MML_COLUMNS);
    printf("    -t\t\tuse %%ticks instead of beats\n");

    return -1;
}

#endif

int demucc(struct pmd* pmd)
{
    int i;
    uint8_t* p;

    p = pmd->buffer;

    if((pmd->num_parts = read_u8(&p)) == 0) {
        pmd->num_parts = read_u8(&p);
    }

    pmd->parts = calloc(pmd->num_parts, sizeof(uint8_t*));

    for(i = 0; i < pmd->num_parts; i++) {
        uint16_t addr = read_u16(&p);
        if(addr != 0) {
            pmd->parts[i] = pmd->buffer + addr;
        }
    }

    pmd->drums = pmd->buffer + read_u16(&p);
    pmd->title = (char*)(pmd->buffer + read_u16(&p));
    pmd->title2 = (char*)(pmd->buffer + read_u16(&p));

    if(pmd->title != (char*)pmd->buffer) {
        char* title = NULL;

        crlf2semicolon(pmd->title, &title);
        printf("#Title %s\n", title);
        free(title);
    }
    if(pmd->title2 != (char*)pmd->buffer) {
        char* title = NULL;

        crlf2semicolon(pmd->title2, &title);
        printf("#Title2 %s\n", title);
        free(title);
    }

    pmd->mmlbuf = mmlbuf_new();

    extract_drums(pmd);

    for(i = 0; i < pmd->num_parts; i++) {
        uint8_t* p = pmd->parts[i];
        char partname = 'A' + i;
        int return_addr;

        if(pmd->parts[i] == NULL)
            continue;
       
        /* pass 1 */
        reset_part_ctx(pmd);
        pmd->part = i;
        mmlbuf_reset(pmd->mmlbuf);
        read_notes(pmd, &p);
        // save return addr
        return_addr = pmd->return_addr;

        /* pass 2 */
        p = pmd->parts[i];
        reset_part_ctx(pmd);
        pmd->part = i;
        pmd->return_addr = return_addr;
        mmlbuf_reset(pmd->mmlbuf);

        mml_printf(pmd, "%c ", partname);
        read_notes(pmd, &p);
        printf("%s\n\n", mmlbuf_buf(pmd->mmlbuf));
    }

    return 0;
}


