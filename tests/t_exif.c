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

#include "conv.h"

/* "Exif\0\0", little-endian TIFF, IFD0 with one orientation entry. */
static const uint8_t exif_le[] = {
	'E', 'x', 'i', 'f', 0, 0,
	'I', 'I', 0x2a, 0, 8, 0, 0, 0,
	1, 0,
	0x12, 0x01, 3, 0, 1, 0, 0, 0, 6, 0, 0, 0,
	0, 0, 0, 0,
};

static const uint8_t exif_be[] = {
	'E', 'x', 'i', 'f', 0, 0,
	'M', 'M', 0, 0x2a, 0, 0, 0, 8,
	0, 1,
	0x01, 0x12, 0, 3, 0, 0, 0, 1, 0, 8, 0, 0,
	0, 0, 0, 0,
};

ATF_TC(orientation);
ATF_TC_HEAD(orientation, tc)
{
	atf_tc_set_md_var(tc, "descr", "orientation in both byte orders");
}
ATF_TC_BODY(orientation, tc)
{

	ATF_CHECK_EQ(exif_orientation(exif_le, sizeof(exif_le)), 6);
	ATF_CHECK_EQ(exif_orientation(exif_be, sizeof(exif_be)), 8);
}

ATF_TC(truncated);
ATF_TC_HEAD(truncated, tc)
{
	atf_tc_set_md_var(tc, "descr", "short or bad data yields 1");
}
ATF_TC_BODY(truncated, tc)
{
	uint8_t bad[sizeof(exif_le)];
	size_t n;

	/* Every prefix shorter than the whole entry. */
	for (n = 0; n < 6 + 8 + 2 + 12; n++)
		ATF_CHECK_EQ(exif_orientation(exif_le, n), 1);
	memcpy(bad, exif_le, sizeof(bad));
	bad[10] = 0xff;				/* IFD offset far away */
	ATF_CHECK_EQ(exif_orientation(bad, sizeof(bad)), 1);
	memcpy(bad, exif_le, sizeof(bad));
	bad[6 + 8 + 2 + 8] = 9;			/* invalid value */
	ATF_CHECK_EQ(exif_orientation(bad, sizeof(bad)), 1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, orientation);
	ATF_TP_ADD_TC(tp, truncated);
	return atf_no_error();
}
