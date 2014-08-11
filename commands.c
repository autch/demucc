/* -*- mode: c; encoding: utf-8-unix; -*- */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "demucc.h"

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
		mml_printf(pmd, "q%d", -(24 * gate / 99 - 24));
		break;
	}
	case 0x02:
		// jump
        pmd->return_addr = read_u16(pp);
		return 1;
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

		mml_printf(pmd, "[");
		break;
	}
	case 0x05:
	{
		// next
		int sp = pmd->sp - 3;
		uint8_t times = pmd->stack[sp];
		pmd->sp = sp;
		// スタックはループ回数を覚えるためだけに使ったので、ジャンプはしない

		mml_printf(pmd, "]%d", times);
		break;
	}
	case 0x06:
		// transpose
		mml_printf(pmd, "_%d", (int8_t)read_u8(pp));
		break;
	case 0x07:
		// tempo
		mml_printf(pmd, "T%d", read_u8(pp));
		break;
	case 0x08:
		// inst
		mml_printf(pmd, "@%d", read_u8(pp));
		break;
	case 0x09:
		// volume
		mml_printf(pmd, "V%d", read_u8(pp));
		break;
	case 0x0a:
	{
		// envelope
		int ar, dr, sl, sr;
		ar = read_u8(pp);
		dr = read_u8(pp);
		sl = read_u8(pp);
		sr = read_u8(pp);
		mml_printf(pmd, "E%d,%d,%d,%d", ar, dr, sl, sr);
		break;
	}
	case 0x0b:
		// detune
		mml_printf(pmd, "D%d", (int8_t)read_u8(pp));
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
        pmd->porpd = (int8_t)read_u8(pp);
        (void)read_u8(pp); // 捨てる
		break;
	case 0x0e:
		// portamento on
		mml_printf(pmd, "{");
        pmd->porsw = 1;
		break;
	case 0x0f:
    {
		// portamento off
        char beats[16];
        pmd->porsw = 0;
        tick2beat(pmd->porlen, beats);
		mml_printf(pmd, "}%s", beats);

        pmd->tick = (pmd->tick + pmd->porlen) % TIMEBASE;
        if(pmd->tick == 0) {
            mml_printf(pmd, " ");
        }
		break;
    }
	case 0x10:
		// パート属性
		pmd->track_attr = read_u8(pp);
		mml_printf(pmd, "=%d", pmd->track_attr); // FIXME: 1 以外は未実装
		break;
	case 0x11:
	{
		// vibrato
		int depth, speed, delay, rate;
		depth = read_u8(pp);
		speed = read_u8(pp);
		delay = read_u8(pp);
		rate = read_u8(pp);
		mml_printf(pmd, "Y%d,%d,%d,%d", depth, speed, delay, rate);
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
		mml_printf(pmd, ":");
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
		mml_printf(pmd, "v%d", read_u8(pp));
		break;
	case 0x1b:
	{
		int8_t vol = read_u8(pp);
		// rel. expression
		if(vol >= 0) {
			mml_printf(pmd, ")%d", vol);
		} else {
			mml_printf(pmd, "(%d", -vol);
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
