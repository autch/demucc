/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

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

int tick2beat(int tick, char* notes)
{
    char* p = notes;
    int curlen = TIMEBASE;
    int meas = 1;
    int b;

    if(tick != 0) {
        b = TIMEBASE / tick;
        if(b != 0 && TIMEBASE / b == tick) {
            sprintf(p, "%d", b);
            return 0;
        }
    }
    
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
            *p++ = '.';
        }
        curlen /= 2;
        meas *= 2;
    }

    *p++ = '\0';

    return 0;
}

void get_note(struct pmd* pmd, int note, int tick)
{
	// o4c = 60
	const static char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
	int k = note % 12, oct = (note - 12) / 12;
    int oct_diff;
    char beats[16];

    if(pmd->porpd != 0) {
        int porpd = pmd->porpd;
        pmd->porpd = 0;
        get_note(pmd, note + porpd, tick);
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
        tick2beat(tick, beats);
        if(pmd->porsw) beats[0] = '\0';
		mml_printf(pmd, "%s%s%s", (note == -1) ? "r" : key[k], beats, pmd->legato ? "&" : "");

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
    pmd->tick = 0;
    pmd->track_attr = 0;
    pmd->sp = 0;
    pmd->len = 4;
    pmd->oct = -1;
    pmd->porsw = 0;
    pmd->porpd = 0;
    pmd->porlen = 0;
    pmd->column = 0;
    pmd->return_addr = 0;
}

int mml_vprintf(struct pmd* pmd, char* format, va_list ap)
{
    va_list aq;
    int ret, required;

    va_copy(aq, ap);
    required = vsnprintf(NULL, 0, format, aq);
    va_end(aq);
    if(pmd->part >= 0 && pmd->column + required >= MML_COLUMNS) {
        ret = mmlbuf_append(pmd->mmlbuf, "\n%c ", 'A' + pmd->part);
        if(ret < 0) return ret;
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

int mml_printf(struct pmd* pmd, char* format, ...)
{
    va_list ap;
    int ret;

    va_start(ap, format);
    ret = mml_vprintf(pmd, format, ap);
    va_end(ap);
    return ret;
}

