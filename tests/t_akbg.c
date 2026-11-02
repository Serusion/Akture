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

#include <stdint.h>
#include <string.h>

#include <atf-c.h>

#include "akbg.h"

static void
header(uint8_t *hdr, uint32_t w, uint32_t h)
{
	struct akbg_header ah;

	akbg_header_init(&ah, w, h);
	akbg_header_encode(&ah, hdr);
}

static void
put32(uint8_t *p, uint32_t v)
{

	p[0] = v & 0xff;
	p[1] = (v >> 8) & 0xff;
	p[2] = (v >> 16) & 0xff;
	p[3] = v >> 24;
}

ATF_TC(valid);
ATF_TC_HEAD(valid, tc)
{
	atf_tc_set_md_var(tc, "descr", "a correct header is accepted");
}
ATF_TC_BODY(valid, tc)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header h;

	header(hdr, 1366, 768);
	ATF_REQUIRE_EQ(akbg_check(hdr, 32 + 1366 * 4 * 768, 1366, 768, &h),
	    AKBG_OK);
	ATF_CHECK_EQ(h.ah_width, 1366);
	ATF_CHECK_EQ(h.ah_height, 768);
	ATF_CHECK_EQ(h.ah_stride, 1366 * 4);
}

ATF_TC(invalid);
ATF_TC_HEAD(invalid, tc)
{
	atf_tc_set_md_var(tc, "descr", "every check of the format applies");
}
ATF_TC_BODY(invalid, tc)
{
	const uint64_t size = 32 + 64 * 4 * 32;
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header h;

	header(hdr, 64, 32);
	hdr[0] = 'X';
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_EMAGIC);

	header(hdr, 64, 32);
	put32(hdr + 8, 2);
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_EVERSION);

	header(hdr, 64, 32);
	put32(hdr + 24, 2);
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_EFORMAT);

	header(hdr, 64, 32);
	put32(hdr + 28, 1);
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_ERESERVED);

	header(hdr, 0, 32);
	ATF_CHECK_EQ(akbg_check(hdr, 32, 0, 32, &h), AKBG_EDIM);
	header(hdr, AKBG_DIMMAX + 1, 1);
	ATF_CHECK_EQ(akbg_check(hdr, size, AKBG_DIMMAX + 1, 1, &h),
	    AKBG_EDIM);

	header(hdr, 64, 32);
	put32(hdr + 20, 64 * 4 - 4);
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_ESTRIDE);
	put32(hdr + 20, 64 * 4 + 2);
	ATF_CHECK_EQ(akbg_check(hdr, size, 64, 32, &h), AKBG_ESTRIDE);

	header(hdr, 64, 32);
	ATF_CHECK_EQ(akbg_check(hdr, size - 1, 64, 32, &h), AKBG_ESIZE);
	ATF_CHECK_EQ(akbg_check(hdr, size + 1, 64, 32, &h), AKBG_ESIZE);

	header(hdr, 64, 32);
	ATF_CHECK_EQ(akbg_check(hdr, size, 32, 64, &h), AKBG_EMISMATCH);
}

ATF_TC(overflow);
ATF_TC_HEAD(overflow, tc)
{
	atf_tc_set_md_var(tc, "descr", "huge strides do not wrap around");
}
ATF_TC_BODY(overflow, tc)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header h;

	/* 0xfffffffc * 16384 would wrap a 32-bit size computation. */
	header(hdr, 16, AKBG_DIMMAX);
	put32(hdr + 20, 0xfffffffcU);
	ATF_CHECK_EQ(akbg_check(hdr, 32 + 16 * 4 * AKBG_DIMMAX, 16,
	    AKBG_DIMMAX, &h), AKBG_ESIZE);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, valid);
	ATF_TP_ADD_TC(tp, invalid);
	ATF_TP_ADD_TC(tp, overflow);
	return atf_no_error();
}
