/*
 *   Copyright © 2008-2009 dragchan <zgchan317@gmail.com>
 *   This file is part of FbTerm.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/input.h>

enum Keys {
	AC_START = K(KT_LATIN, 0x80),
	SHIFT_PAGEUP = AC_START,
	SHIFT_PAGEDOWN,
	SHIFT_LEFT,
	SHIFT_RIGHT,
	CTRL_SPACE,
	CTRL_ALT_1,
	CTRL_ALT_2,
	CTRL_ALT_3,
	CTRL_ALT_4,
	CTRL_ALT_5,
	CTRL_ALT_6,
	CTRL_ALT_7,
	CTRL_ALT_8,
	CTRL_ALT_9,
	CTRL_ALT_0,
	CTRL_ALT_C,
	CTRL_ALT_D,
	CTRL_ALT_E,
	CTRL_ALT_F1,
	CTRL_ALT_F2,
	CTRL_ALT_F3,
	CTRL_ALT_F4,
	CTRL_ALT_F5,
	CTRL_ALT_F6,
	AC_END = CTRL_ALT_F6,
};

static char key_down[NR_KEYS];
static unsigned char shift_down[NR_SHIFT];
static short shift_state;
static char lock_state;
static char cr_with_lf, applic_keypad, cursor_esco;
static int npadch;

void init_keycode_state()
{
	npadch = -1;
	shift_state = 0;
	memset(key_down, 0, sizeof(char) * NR_KEYS);
	memset(shift_down, 0, sizeof(char) * NR_SHIFT);
	ioctl(STDIN_FILENO, KDGKBLED, &lock_state);
}

void update_term_mode(char crlf, char appkey, char curo)
{
    cr_with_lf = crlf;
    applic_keypad = appkey;
    cursor_esco = curo;
}

unsigned short keycode_to_keysym(unsigned short keycode, char down)
{
	if (keycode >= NR_KEYS) return K_HOLE;

	char rep = (down && key_down[keycode]);
	key_down[keycode] = down;

	struct kbentry ke;
	ke.kb_table = shift_state;
	ke.kb_index = keycode;

	if (ioctl(STDIN_FILENO, KDGKBENT, &ke) == -1) return K_HOLE;

	if (KTYP(ke.kb_value) == KT_LETTER && (lock_state & K_CAPSLOCK)) {
		ke.kb_table = shift_state ^ (1 << KG_SHIFT);
		if (ioctl(STDIN_FILENO, KDGKBENT, &ke) == -1) return K_HOLE;
	}

	if (ke.kb_value == K_HOLE || ke.kb_value == K_NOSUCHMAP) return K_HOLE;

	unsigned value = KVAL(ke.kb_value);

	switch (KTYP(ke.kb_value)) {
	case KT_SPEC:
		switch (ke.kb_value) {
		case K_NUM:
			if (applic_keypad) break;
		case K_BARENUMLOCK:
		case K_CAPS:
		case K_CAPSON:
			if (down && !rep) {
				if (value == KVAL(K_NUM) || value == KVAL(K_BARENUMLOCK)) lock_state ^= K_NUMLOCK;
				else if (value == KVAL(K_CAPS)) lock_state ^= K_CAPSLOCK;
				else if (value == KVAL(K_CAPSON)) lock_state |= K_CAPSLOCK;

				ioctl(STDIN_FILENO, KDSKBLED, lock_state);
			}
			break;

		default:
			break;
		}
		break;

	case KT_SHIFT:
		if (value >= NR_SHIFT || rep) break;

		if (value == KVAL(K_CAPSSHIFT)) {
			value = KVAL(K_SHIFT);

			if (down && (lock_state & K_CAPSLOCK)) {
				lock_state &= ~K_CAPSLOCK;
				ioctl(STDIN_FILENO, KDSKBLED, lock_state);
			}
		}

		if (down) shift_down[value]++;
		else if (shift_down[value]) shift_down[value]--;

		if (shift_down[value]) shift_state |= (1 << value);
		else shift_state &= ~(1 << value);

		break;

	case KT_LATIN:
	case KT_LETTER:
	case KT_FN:
	case KT_PAD:
	case KT_CONS:
	case KT_CUR:
	case KT_META:
	case KT_ASCII:
		break;

	default:
		printf("not support!\n");
		break;
	}

	return ke.kb_value;
}

unsigned short keypad_keysym_redirect(unsigned short keysym)
{
	if (applic_keypad || KTYP(keysym) != KT_PAD || KVAL(keysym) >= NR_PAD) return keysym;

	#define KL(val) K(KT_LATIN, val)
	static const unsigned short num_map[] = {
		KL('0'), KL('1'), KL('2'), KL('3'), KL('4'),
		KL('5'), KL('6'), KL('7'), KL('8'), KL('9'),
		KL('+'), KL('-'), KL('*'), KL('/'), K_ENTER,
		KL(','), KL('.'), KL('?'), KL('('), KL(')'),
		KL('#')
	};

	static const unsigned short fn_map[] = {
		K_INSERT, K_SELECT, K_DOWN, K_PGDN, K_LEFT,
		K_P5, K_RIGHT, K_FIND, K_UP, K_PGUP,
		KL('+'), KL('-'), KL('*'), KL('/'), K_ENTER,
		K_REMOVE, K_REMOVE, KL('?'), KL('('), KL(')'),
		KL('#')
	};

	if (lock_state & K_NUMLOCK) return num_map[keysym - K_P0];
	return fn_map[keysym - K_P0];
}


static unsigned to_utf8(unsigned c, char *buf)
{
	unsigned index = 0;

	if (c < 0x80)
		buf[index++] = c;
	else if (c < 0x800) {
		// 110***** 10******
		buf[index++] = 0xc0 | (c >> 6);
		buf[index++] = 0x80 | (c & 0x3f);
	} else if (c < 0x10000) {
		if (c >= 0xD800 && c < 0xE000)
			return index;
		if (c == 0xFFFF)
			return index;
		// 1110**** 10****** 10******
		buf[index++] = 0xe0 | (c >> 12);
		buf[index++] = 0x80 | ((c >> 6) & 0x3f);
		buf[index++] = 0x80 | (c & 0x3f);
	} else if (c < 0x200000) {
		// 11110*** 10****** 10****** 10******
		buf[index++] = 0xf0 | (c >> 18);
		buf[index++] = 0x80 | ((c >> 12) & 0x3f);
		buf[index++] = 0x80 | ((c >> 6) & 0x3f);
		buf[index++] = 0x80 | (c & 0x3f);
	}

	return index;
}


char *keysym_to_term_string(unsigned short keysym, char down)
{
	static struct kbsentry kse;
	char *buf = (char *)kse.kb_string;
	*buf = 0;

	if (KTYP(keysym) != KT_SHIFT && !down) return buf;

	keysym = keypad_keysym_redirect(keysym);
	unsigned index = 0, value = KVAL(keysym);


	switch (KTYP(keysym)) {
	case KT_LATIN:
	case KT_LETTER:
		if (value < KVAL(AC_START) || value > KVAL(AC_END)) index = to_utf8(value, buf);
		break;

	case KT_FN:
		kse.kb_func = value;
		ioctl(STDIN_FILENO, KDGKBSENT, &kse);
		index = strlen(buf);
		break;

	case KT_SPEC:
		if (keysym == K_ENTER) {
			buf[index++] = '\r';
			if (cr_with_lf) buf[index++] = '\n';
		} else if (keysym == K_NUM && applic_keypad) {
			buf[index++] = '\e';
			buf[index++] = 'O';
			buf[index++] = 'P';
		}
		break;

	case KT_PAD:
		if (applic_keypad && !shift_down[KG_SHIFT]) {
			if (value < NR_PAD) {
				static const char app_map[] = "pqrstuvwxylSRQMnnmPQS";

				buf[index++] = '\e';
				buf[index++] = 'O';
				buf[index++] = app_map[value];
			}
		} else if (keysym == K_P5 && !(lock_state & K_NUMLOCK)) {
			buf[index++] = '\e';
			buf[index++] = (applic_keypad ? 'O' : '[');
			buf[index++] = 'G';
		}
		break;

	case KT_CUR:
		if (value < 4) {
			static const char cur_chars[] = "BDCA";

			buf[index++] = '\e';
			buf[index++] = (cursor_esco ? 'O' : '[');
			buf[index++] = cur_chars[value];
		}
		break;

	case KT_META: {
		long flag;
		ioctl(STDIN_FILENO, KDGKBMETA, &flag);

		if (flag == K_METABIT) {
			buf[index++] = 0x80 | value;
		} else {
			buf[index++] = '\e';
			buf[index++] = value;
		}
		break;
	}

	case KT_SHIFT:
		if (!down && npadch != -1) {
			index = to_utf8(npadch, buf);
			npadch = -1;
		}
		break;

	case KT_ASCII:
		if (value < NR_ASCII) {
			int base = 10;

			if (value >= KVAL(K_HEX0)) {
				base = 16;
				value -= KVAL(K_HEX0);
			}

			if (npadch == -1) npadch = value;
			else npadch = npadch * base + value;
		}
		break;

	default:
		break;
	}

	buf[index] = 0;
	return buf;
}
