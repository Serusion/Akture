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

#include <sys/types.h>
#include <sys/endian.h>

#include <stdint.h>
#include <string.h>

#include "akbg.h"

static const uint8_t akbg_magic[AKBG_MAGICLEN] = {
	'A', 'K', 'T', 'U', 'R', 'E', 'B', 'G'
};

/*
 * Validate the header of an .akbg file before any pixel is touched.
 * hdr holds the first AKBG_HDRLEN bytes, fsize is the size of the
 * whole file and outw x outh the resolution of the output it is for.
 * The function only computes; it is the target of Frama-C (Eva and
 * WP) and of the fuzzer in tests/fuzz.  It compares and fills in
 * field by field, without memcmp(3) or memset(3), to stay within the
 * typed memory model of WP.
 */
/*@
  requires \valid_read(hdr + (0 .. AKBG_HDRLEN - 1));
  requires \valid(h);
  requires \separated(h, hdr + (0 .. AKBG_HDRLEN - 1));
  assigns *h;
  ensures AKBG_OK <= \result <= AKBG_ERESERVED;
  ensures \result == AKBG_OK ==>
    1 <= h->ah_width <= AKBG_DIMMAX && 1 <= h->ah_height <= AKBG_DIMMAX &&
    (uint64_t)h->ah_width * 4 <= h->ah_stride &&
    AKBG_HDRLEN + (uint64_t)h->ah_stride * h->ah_height == fsize;
*/
int
akbg_check(const uint8_t *hdr, uint64_t fsize, uint32_t outw, uint32_t outh,
    struct akbg_header *h)
{
	uint64_t need;
	size_t i;

	h->ah_version = le32dec(hdr + 8);
	h->ah_width = le32dec(hdr + 12);
	h->ah_height = le32dec(hdr + 16);
	h->ah_stride = le32dec(hdr + 20);
	h->ah_format = le32dec(hdr + 24);

	/*@
	  loop invariant 0 <= i <= AKBG_MAGICLEN;
	  loop assigns i;
	  loop variant AKBG_MAGICLEN - i;
	*/
	for (i = 0; i < AKBG_MAGICLEN; i++) {
		if (hdr[i] != akbg_magic[i])
			return AKBG_EMAGIC;
	}

	if (h->ah_version != AKBG_VERSION)
		return AKBG_EVERSION;
	if (h->ah_format != AKBG_FMT_BGRX8888)
		return AKBG_EFORMAT;
	if (le32dec(hdr + 28) != 0)
		return AKBG_ERESERVED;
	if (h->ah_width < 1 || h->ah_width > AKBG_DIMMAX ||
	    h->ah_height < 1 || h->ah_height > AKBG_DIMMAX)
		return AKBG_EDIM;
	/* Rows are mapped as 32-bit pixels, so keep them aligned. */
	if ((uint64_t)h->ah_width * 4 > h->ah_stride || h->ah_stride % 4 != 0)
		return AKBG_ESTRIDE;

	/*
	 * stride < 2^32 and height <= 2^14, so the product fits in 64
	 * bits with room to spare.
	 */
	need = AKBG_HDRLEN + (uint64_t)h->ah_stride * h->ah_height;
	if (need != fsize)
		return AKBG_ESIZE;
	if (h->ah_width != outw || h->ah_height != outh)
		return AKBG_EMISMATCH;
	return AKBG_OK;
}

void
akbg_header_init(struct akbg_header *h, uint32_t width, uint32_t height)
{

	h->ah_version = AKBG_VERSION;
	h->ah_width = width;
	h->ah_height = height;
	h->ah_stride = width * 4;
	h->ah_format = AKBG_FMT_BGRX8888;
}

void
akbg_header_encode(const struct akbg_header *h, uint8_t *hdr)
{

	(void)memset(hdr, 0, AKBG_HDRLEN);
	(void)memcpy(hdr, AKBG_MAGIC, AKBG_MAGICLEN);
	le32enc(hdr + 8, h->ah_version);
	le32enc(hdr + 12, h->ah_width);
	le32enc(hdr + 16, h->ah_height);
	le32enc(hdr + 20, h->ah_stride);
	le32enc(hdr + 24, h->ah_format);
}

const char *
akbg_strerror(int error)
{

	switch (error) {
	case AKBG_OK:
		return "no error";
	case AKBG_EMAGIC:
		return "not an akbg file";
	case AKBG_EVERSION:
		return "unsupported version";
	case AKBG_EFORMAT:
		return "unsupported pixel format";
	case AKBG_EDIM:
		return "dimensions out of range";
	case AKBG_ESTRIDE:
		return "bad row length";
	case AKBG_ESIZE:
		return "file size mismatch";
	case AKBG_EMISMATCH:
		return "resolution does not match the output";
	case AKBG_ERESERVED:
		return "reserved field not zero";
	default:
		return "unknown error";
	}
}
