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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <atf-c.h>

#include "autuni.h"

#define W	128
#define H	64

ATF_TC(utf8);
ATF_TC_HEAD(utf8, tc)
{
	atf_tc_set_md_var(tc, "descr", "characters of UTF-8 strings");
}
ATF_TC_BODY(utf8, tc)
{
	const char *s = "a\xe3\x81\x82" "b";	/* a, HIRAGANA A, b */

	ATF_CHECK_EQ(au_utf8_len(s, 5), 3);
	ATF_CHECK_EQ(au_utf8_len(s, 4), 2);
	ATF_CHECK_EQ(au_utf8_prev(s, 5), 4);
	ATF_CHECK_EQ(au_utf8_prev(s, 4), 1);
	ATF_CHECK_EQ(au_utf8_prev(s, 1), 0);
	ATF_CHECK_EQ(au_utf8_prev(s, 0), 0);
	/* A malformed byte counts as one character. */
	ATF_CHECK_EQ(au_utf8_len("\xff" "a", 2), 2);
}

ATF_TC(text);
ATF_TC_HEAD(text, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "text in the font that AUTUNI_TEST_FONT names");
}
ATF_TC_BODY(text, tc)
{
	struct au_canvas c;
	struct au_font f;
	struct au_face *face;
	pixman_image_t *img;
	const uint32_t *px;
	const char *path;
	double w, x;
	int i, inked;

	if ((path = getenv("AUTUNI_TEST_FONT")) == NULL)
		atf_tc_skip("AUTUNI_TEST_FONT names no font");
	ATF_REQUIRE_EQ(au_font_init(), 0);
	ATF_CHECK(au_face_open("/nonexistent/font.ttf") == NULL);
	ATF_REQUIRE((face = au_face_open(path)) != NULL);
	f.f_face = face;
	f.f_fallback = face;
	f.f_size = 20.0;
	f.f_weight = 400;

	w = au_font_width(&f, "Autumn");
	ATF_CHECK(w > 0);
	ATF_CHECK(au_font_width(&f, "Autumn Autumn") > 2 * w);
	ATF_CHECK(au_font_ascent(&f) > 0);
	ATF_CHECK(au_font_descent(&f) > 0);

	img = pixman_image_create_bits(PIXMAN_x8r8g8b8, W, H, NULL, W * 4);
	ATF_REQUIRE(img != NULL);
	(void)memset(pixman_image_get_data(img), 0, (size_t)W * H * 4);
	c.c_img = img;
	pixman_region32_init_rect(&c.c_clip, 0, 0, W, H);
	pixman_image_set_clip_region32(img, &c.c_clip);
	x = au_font_draw(&c, &f, 4.0, 40.0, "Autumn", 0xffffffffU);
	ATF_CHECK_MSG(fabs(x - (4.0 + w)) < 1e-9, "x %f, width %f", x, w);

	px = pixman_image_get_data(img);
	for (i = inked = 0; i < W * H; i++)
		inked += (px[i] & 0x00ffffffU) != 0;
	ATF_CHECK(inked > 0);
	pixman_image_unref(img);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, utf8);
	ATF_TP_ADD_TC(tp, text);
	return atf_no_error();
}
