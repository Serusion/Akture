/*-
 * Copyright (c) 2026 Serusion
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _AKOPT_H_
#define _AKOPT_H_

/*
 * Keys of akture.conf(5).  The numbers double as the key of
 * AKMSG_INIT_OPTION, so append new keys at the end.
 */
#define AKOPT_BACKGROUND	0
#define AKOPT_BACKGROUND_COLOR	1
#define AKOPT_ACCENT		2
#define AKOPT_TEXT_COLOR	3
#define AKOPT_FONT_UI		4
#define AKOPT_FONT_CLOCK	5
#define AKOPT_CLOCK_FORMAT	6
#define AKOPT_SHOW_LOGO		7
#define AKOPT_LOGO		8
#define AKOPT_SCALE		9
#define AKOPT_KEYBOARD_LAYOUTS	10
#define AKOPT_DEFAULT_SESSION	11
#define AKOPT_REMEMBER_USER	12
#define AKOPT_ALLOW_POWER	13
#define AKOPT_AUTOLOGIN_USER	14
#define AKOPT_X_SERVER		15
#define AKOPT_X_ARGS		16
#define AKOPT_PREFER_XSESSION	17
#define AKOPT_NKEYS		18

/* Value types */
#define AKOPT_T_STRING		0	/* printable text */
#define AKOPT_T_NAME		1	/* name without '/' or blanks */
#define AKOPT_T_PATH		2	/* absolute path */
#define AKOPT_T_COLOR		3	/* #RRGGBB */
#define AKOPT_T_BOOL		4	/* yes or no */
#define AKOPT_T_SCALE		5	/* 1, 1.25, 1.5 or 2 */
#define AKOPT_T_LAYOUTS		6	/* comma separated xkb layouts */

/* Flags */
#define AKOPT_F_GREETER		0x01	/* sent to the greeter in INIT */

struct akopt {
	const char	*ao_name;
	int		 ao_type;
	int		 ao_flags;
	const char	*ao_default;
};

extern const struct akopt akopt_table[AKOPT_NKEYS];

int	akopt_lookup(const char *);
int	akopt_valid(int, const char *);

#endif /* !_AKOPT_H_ */
