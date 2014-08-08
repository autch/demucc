/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"
#include <stdio.h>
#include <string.h>

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

// FIXME: n連符ってこれで出る？
int tick2beat(int tick, char* notes)
{
    char* p = notes;
    int curlen = TPQN * 4;
    int meas = 1;

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

    return 0;
}

void get_note(struct pmd* pmd, int note, int tick)
{
	// o4c = 60
	char* key[] = {"c", "c+", "d", "d+", "e", "f", "f+", "g", "g+", "a", "a+", "b"};
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
    pmd->porpd = 0;
    pmd->porlen = 0;
}
