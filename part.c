/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"

/**
 * @return パート終了のとき 1, そうでないとき 0
 */
int read_notes(struct pmd* pmd, uint8_t** pp)
{
    uint8_t n;
    while(1) {
        if(pmd->return_addr != 0
           && (*pp - pmd->buffer) == pmd->return_addr) {
            mml_printf(pmd, " L ");
            pmd->return_addr = 0;
        }

        n = read_u8(pp);

        if(n >= 0xe0) {
            if(read_commands(pmd, n, pp) != 0) {
                return 1;
            }
        } else if(n >= 0x80) {
            if(pmd->track_attr & TRATTR_DRUMS) {
                int index = n - 0x80;
                char drumname[DRUMNAME_MAX];
                char beats[16];

                get_drumname(index, drumname, sizeof drumname);
                tick2beat(pmd->len, beats);
                mml_printf(pmd, "!%sg%s", drumname, beats);
                
                pmd->tick = (pmd->tick + pmd->len) % TIMEBASE;
                if(pmd->tick == 0) {
                    mml_printf(pmd, " ");
                }
            } else {
                int note = n - 0x80 + 12;
                get_note(pmd, note, pmd->len, 0);
            }                   
        } else if(n != 0) {
            if(n == 127) {
                n = read_u8(pp);
                if(n < 127) n += 256;
            }
            if(pmd->porsw != 0) {
                pmd->porlen = pmd->len = n;
            } else {
                if(pmd->len != n) {
                    pmd->len = n;
                    // mucc は l で付点を書くとバグるので、ここでは何もしない
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
                mml_newline(pmd);
            }
        }
    }
    return 0;
}
