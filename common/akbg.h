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

#ifndef _AKBG_H_
#define _AKBG_H_

#include <stdint.h>

/*
 * The .akbg background format: a 32-byte header followed by pixels
 * that can be handed to the X server as they are.
 *
 *	0	"AKTUREBG"
 *	8	version (1)
 *	12	width
 *	16	height
 *	20	bytes per row
 *	24	pixel format (1 = BGRX 8888)
 *	28	reserved (0)
 *	32	pixel data
 *
 * All numbers are little-endian 32-bit unsigned.
 */
#define AKBG_MAGIC		"AKTUREBG"
#define AKBG_MAGICLEN		8
#define AKBG_HDRLEN		32
#define AKBG_VERSION		1
#define AKBG_FMT_BGRX8888	1
#define AKBG_DIMMAX		16384
#define AKBG_SUFFIX		".akbg"

struct akbg_header {
	uint32_t	ah_version;
	uint32_t	ah_width;
	uint32_t	ah_height;
	uint32_t	ah_stride;
	uint32_t	ah_format;
};

/* akbg_check() errors */
#define AKBG_OK		0
#define AKBG_EMAGIC	1	/* not an .akbg file */
#define AKBG_EVERSION	2	/* unsupported version */
#define AKBG_EFORMAT	3	/* unsupported pixel format */
#define AKBG_EDIM	4	/* width or height out of range */
#define AKBG_ESTRIDE	5	/* row length too small or unaligned */
#define AKBG_ESIZE	6	/* file size does not match */
#define AKBG_EMISMATCH	7	/* not the resolution of the output */
#define AKBG_ERESERVED	8	/* reserved field not zero */

int	akbg_check(const uint8_t *, uint64_t, uint32_t, uint32_t,
	    struct akbg_header *);
void	akbg_header_init(struct akbg_header *, uint32_t, uint32_t);
void	akbg_header_encode(const struct akbg_header *, uint8_t *);
const char *akbg_strerror(int);

#endif /* !_AKBG_H_ */
