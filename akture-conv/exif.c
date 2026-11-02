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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "conv.h"

/*
 * The orientation tag of an EXIF APP1 segment.  The input is
 * untrusted, so every offset is checked against the length.
 */

#define EXIF_ORIENTATION	0x0112
#define EXIF_SHORT		3

static uint16_t
exif_u16(const uint8_t *p, int be)
{

	return be ? (uint16_t)(p[0] << 8 | p[1]) : (uint16_t)(p[1] << 8 | p[0]);
}

static uint32_t
exif_u32(const uint8_t *p, int be)
{

	return be ?
	    (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
	    p[3] :
	    (uint32_t)p[3] << 24 | (uint32_t)p[2] << 16 | (uint32_t)p[1] << 8 |
	    p[0];
}

/*
 * Return the orientation (1 .. 8) from the APP1 payload, which starts
 * with "Exif\0\0".  Anything unexpected yields 1, i.e. as stored.
 */
int
exif_orientation(const uint8_t *buf, size_t len)
{
	const uint8_t *tiff;
	size_t tlen, ifd, e;
	uint16_t n, i, v;
	int be;

	if (len < 6 || memcmp(buf, "Exif\0\0", 6) != 0)
		return 1;
	tiff = buf + 6;
	tlen = len - 6;
	if (tlen < 8)
		return 1;
	if (memcmp(tiff, "II*\0", 4) == 0)
		be = 0;
	else if (memcmp(tiff, "MM\0*", 4) == 0)
		be = 1;
	else
		return 1;

	ifd = exif_u32(tiff + 4, be);
	if (ifd > tlen || tlen - ifd < 2)
		return 1;
	n = exif_u16(tiff + ifd, be);
	for (i = 0; i < n; i++) {
		e = ifd + 2 + (size_t)i * 12;
		if (e > tlen || tlen - e < 12)
			return 1;
		if (exif_u16(tiff + e, be) != EXIF_ORIENTATION)
			continue;
		if (exif_u16(tiff + e + 2, be) != EXIF_SHORT ||
		    exif_u32(tiff + e + 4, be) != 1)
			return 1;
		v = exif_u16(tiff + e + 8, be);
		return v >= 1 && v <= 8 ? v : 1;
	}
	return 1;
}
