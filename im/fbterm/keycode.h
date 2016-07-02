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

#ifndef KEYCODE_H
#define KEYCODE_H

#ifdef __cplusplus
extern "C" {
#endif

void init_keycode_state();

void update_term_mode(char crlf, char appkey, char curo);

unsigned short keycode_to_keysym(unsigned short keycode, char down);

unsigned short keypad_keysym_redirect(unsigned short keysym);

char *keysym_to_term_string(unsigned short keysym, char down);

#ifdef __cplusplus
}
#endif

#endif
