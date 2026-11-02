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

/* libFuzzer harness for the .akbg header check. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "akbg.h"

int	LLVMFuzzerTestOneInput(const uint8_t *, size_t);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header h;
	uint64_t fsize;
	uint32_t w, hh;

	if (size < AKBG_HDRLEN + 16)
		return 0;
	memcpy(hdr, data, AKBG_HDRLEN);
	memcpy(&fsize, data + AKBG_HDRLEN, 8);
	memcpy(&w, data + AKBG_HDRLEN + 8, 4);
	memcpy(&hh, data + AKBG_HDRLEN + 12, 4);
	if (akbg_check(hdr, fsize, w, hh, &h) == AKBG_OK &&
	    (uint64_t)h.ah_stride * h.ah_height + AKBG_HDRLEN != fsize)
		__builtin_trap();
	return 0;
}
