/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

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

int tick2beat(struct pmd* pmd, int tick, char* notes)
{
    int initial_tick = tick;
    char* p = notes;
    int curlen = TIMEBASE;
    int meas = 1;
    int b;

    // 全音符より長い
    if(tick > TIMEBASE || pmd->use_ticks) {
        // mucc には ^ がないので直接指定するしかない
        sprintf(notes, "%%%d", tick);
        return 0;
    }

    // n 分音符として割り切れるなら一発
    if(tick != 0) {
        b = TIMEBASE / tick;
        if(b != 0 && TIMEBASE / b == tick
           && TIMEBASE % b == 0
           && TIMEBASE % tick == 0) {
            sprintf(p, "%d", b);
            return 0;
        }
    }
    
    // 最大の音長を引く
    while(tick > 0 && curlen != 0) {
        if(curlen <= tick) {
            tick -= curlen;
            p += sprintf(p, "%d", meas);
            break;
        }
        curlen >>= 1;
        meas <<= 1;
    }
    // 残りを付点で足す
    while(tick > 0 && curlen != 0) {
        if(curlen <= tick) {
            tick -= curlen;
            *p++ = '.';
        } else {
            // 付点で割り切れない→仕方ないので % で表記
            break;
        }
        curlen >>= 1;
    }

    *p++ = '\0';

    // これだけやっても割り切れなかった
    if(curlen == 0 || tick != 0) {
        sprintf(notes, "%%%d", initial_tick);
    }

    return 0;
}

void get_note(struct pmd* pmd, int note, int tick, int por_pre)
{
    // o4c = 60
    const static char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
    int k = note % 12, oct = (note - 12) / 12;
    int oct_diff;
    char beats[16];

    if(por_pre == 0 && pmd->porsw == 1 && note >= 0) {
        get_note(pmd, note + pmd->porpd, tick, 1);
    }

    if(tick > 0) {
        if(note > 0) {
            oct_diff = oct - pmd->oct;
            pmd->oct = oct;
            switch(oct_diff) {
            case 0:
                break;
            case 1:
                mml_printf(pmd, ">");
                break;
            case -1:
                mml_printf(pmd, "<");
                break;
            default:
                mml_printf(pmd, "o%d", oct);
            }
        }
        tick2beat(pmd, tick, beats);
        if(pmd->porsw) beats[0] = '\0';
        mml_printf(pmd, "%s%s%s",
                   (note == -1) ? "r" : key[k],
                   beats,
                   (!pmd->porsw && pmd->legato) ? "&" : "");

        if(pmd->porsw == 0) {
            pmd->tick = (pmd->tick + pmd->len) % TIMEBASE;
            if(pmd->tick == 0) {
                mml_printf(pmd, " ");
            }
        }
    } else {
        // for use in drums
        mml_printf(pmd, "?o%d%s%s", oct, key[k], pmd->legato ? "&" : "");
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
    pmd->sp = 0;
    pmd->len = 4;
    pmd->track_attr = 0;
    pmd->legato = 0;
    pmd->oct = -1;
    pmd->porsw = 0;
    pmd->porpd = 0;
    pmd->porlen = 0;
    pmd->return_addr = 0;
    pmd->tick = 0;

    pmd->column = 0;
    pmd->newline = 0;
}

int mml_vprintf(struct pmd* pmd, char* format, va_list ap)
{
    va_list aq;
    int ret, required;

    va_copy(aq, ap);
    required = vsnprintf(NULL, 0, format, aq);
    va_end(aq);
    if(pmd->part >= 0 && (pmd->newline != 0 || pmd->column + required >= pmd->mml_columns)) {
        ret = mmlbuf_append(pmd->mmlbuf, "\n%c ", 'A' + pmd->part);
        if(ret < 0) return ret;
        pmd->newline = 0;
        pmd->column = ret - 1;
    }
    va_copy(aq, ap);
    ret = mmlbuf_appendv(pmd->mmlbuf, format, aq);
    va_end(aq);
    if(ret >= 0) {
        pmd->column += ret;
    }
    
    return ret;
}

int mml_newline(struct pmd* pmd)
{
    pmd->newline = 1;
    return 0;
}

int mml_printf(struct pmd* pmd, char* format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = mml_vprintf(pmd, format, ap);
    va_end(ap);
    return ret;
}

int crlf2semicolon(char* string, char** ppoutbuf)
{
    size_t size;
    char* outbuf;
    char* p = string;
    char* dp;

    size = strlen(string);
    dp = outbuf = calloc(size + 1, 1);

    while(*p != '\0') {
        if(*p == '\n') {
            *dp++ = ';';
        } else {
            *dp++ = *p;
        }
        p++;
    }
    *dp++ = '\0';

    *ppoutbuf = outbuf;
    
    return p - string;
}
