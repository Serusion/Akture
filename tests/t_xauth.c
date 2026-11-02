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

#include "akture.h"

static const uint8_t cookie[AK_COOKIELEN] = {
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};

ATF_TC(entries);
ATF_TC_HEAD(entries, tc)
{
	atf_tc_set_md_var(tc, "descr", "records follow the Xauthority format");
}
ATF_TC_BODY(entries, tc)
{
	uint8_t buf[256];
	size_t len;

	len = xauth_entries(buf, sizeof(buf), "host", "0", cookie);
	/* 2 + (2+4) + (2+1) + (2+18) + (2+16) = 49, wild: 45 */
	ATF_REQUIRE_EQ(len, 49 + 45);
	ATF_CHECK_EQ(buf[0], 0x01);		/* FamilyLocal, big-endian */
	ATF_CHECK_EQ(buf[1], 0x00);
	ATF_CHECK(memcmp(buf + 4, "host", 4) == 0);
	ATF_CHECK(memcmp(buf + 49 - 16, cookie, 16) == 0);
	ATF_CHECK_EQ(buf[49], 0xff);		/* FamilyWild */

	ATF_CHECK_EQ(xauth_entries(buf, 60, "host", "0", cookie), 0);
}

ATF_TC(filter);
ATF_TC_HEAD(filter, tc)
{
	atf_tc_set_md_var(tc, "descr", "other displays survive a merge");
}
ATF_TC_BODY(filter, tc)
{
	uint8_t in[512], out[512];
	size_t a, b, n;

	a = xauth_entries(in, sizeof(in), "host", "1", cookie);
	b = xauth_entries(in + a, sizeof(in) - a, "host", "0", cookie);
	n = xauth_filter(in, a + b, "0", out, sizeof(out));
	ATF_CHECK_EQ(n, a);
	ATF_CHECK(memcmp(in, out, a) == 0);

	/* A truncated record is dropped, not over-read. */
	n = xauth_filter(in, a + 5, "0", out, sizeof(out));
	ATF_CHECK_EQ(n, a);
	n = xauth_filter(in, 3, "0", out, sizeof(out));
	ATF_CHECK_EQ(n, 0);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, entries);
	ATF_TP_ADD_TC(tp, filter);
	return atf_no_error();
}
