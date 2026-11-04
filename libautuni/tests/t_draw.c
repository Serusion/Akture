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

#include <string.h>

#include <atf-c.h>

#include "autuni.h"

/*
 * The shapes, drawn in white or red on black: where they cover, where
 * they do not, and the antialiased edge in between.
 */

#define W	64
#define H	64
#define WHITE	0xffffffffU
#define RED	0xffff0000U

static pixman_image_t *img;
static struct au_canvas canvas;

/* A black W by H image, with a canvas over all of it. */
static struct au_canvas *
canvas_new(void)
{

	img = pixman_image_create_bits(PIXMAN_x8r8g8b8, W, H, NULL, W * 4);
	ATF_REQUIRE(img != NULL);
	(void)memset(pixman_image_get_data(img), 0, (size_t)W * H * 4);
	canvas.c_img = img;
	pixman_region32_init_rect(&canvas.c_clip, 0, 0, W, H);
	pixman_image_set_clip_region32(img, &canvas.c_clip);
	return &canvas;
}

/* The colour at (x, y), without the unused byte. */
static uint32_t
pixel(int x, int y)
{

	return pixman_image_get_data(img)[y * W + x] & 0x00ffffffU;
}

/* The red channel at (x, y): the coverage of a white or red shape. */
static unsigned int
red(int x, int y)
{

	return (pixel(x, y) >> 16) & 0xff;
}

ATF_TC(alpha);
ATF_TC_HEAD(alpha, tc)
{
	atf_tc_set_md_var(tc, "descr", "au_draw_alpha() replaces the alpha");
}
ATF_TC_BODY(alpha, tc)
{

	ATF_CHECK_EQ(au_draw_alpha(0xff112233U, 0x80), 0x80112233U);
	ATF_CHECK_EQ(au_draw_alpha(0x00112233U, 0x1ff), 0xff112233U);
}

ATF_TC(fill);
ATF_TC_HEAD(fill, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "opaque fills replace, translucent ones blend");
}
ATF_TC_BODY(fill, tc)
{
	struct au_canvas *c = canvas_new();
	struct au_rect r = { 8, 8, 16, 16 };
	unsigned int g;

	au_draw_fill(c, &r, RED);
	ATF_CHECK_EQ(pixel(8, 8), 0xff0000U);
	ATF_CHECK_EQ(pixel(23, 23), 0xff0000U);
	ATF_CHECK_EQ(pixel(24, 24), 0);
	ATF_CHECK_EQ(pixel(7, 8), 0);

	au_draw_fill(c, &r, au_draw_alpha(WHITE, 0x80));
	g = (pixel(10, 10) >> 8) & 0xff;
	ATF_CHECK_EQ(red(10, 10), 0xff);
	ATF_CHECK_MSG(g >= 0x7f && g <= 0x81, "green %#x", g);
}

ATF_TC(clip);
ATF_TC_HEAD(clip, tc)
{
	atf_tc_set_md_var(tc, "descr", "nothing is drawn outside the clip");
}
ATF_TC_BODY(clip, tc)
{
	struct au_canvas *c = canvas_new();
	struct au_rect r = { 0, 0, W, H };

	pixman_region32_fini(&c->c_clip);
	pixman_region32_init_rect(&c->c_clip, 0, 0, W / 2, H);
	pixman_image_set_clip_region32(img, &c->c_clip);
	au_draw_fill(c, &r, RED);
	ATF_CHECK_EQ(pixel(10, 10), 0xff0000U);
	ATF_CHECK_EQ(pixel(W / 2 + 10, 10), 0);
	au_draw_circle(c, 48.0, 48.0, 4.0, WHITE);
	ATF_CHECK_EQ(pixel(48, 48), 0);
}

ATF_TC(shapes);
ATF_TC_HEAD(shapes, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "circle, ring, arc, rounded rectangle and line");
}
ATF_TC_BODY(shapes, tc)
{
	struct au_canvas *c = canvas_new();
	unsigned int v;

	/* A disc with its edge a quarter into the pixel at x = 24. */
	au_draw_circle(c, 16.0, 16.0, 8.25, WHITE);
	ATF_CHECK_EQ(red(16, 16), 0xff);
	ATF_CHECK_EQ(red(16, 28), 0);
	v = red(24, 16);
	ATF_CHECK_MSG(v > 0 && v < 0xff, "edge %#x", v);

	/* A ring is hollow. */
	au_draw_ring(c, 48.0, 16.0, 8.0, 2.0, WHITE);
	ATF_CHECK_EQ(red(48, 16), 0);
	ATF_CHECK(red(56, 16) > 0xc0);

	/* The arc of a power symbol is open at the top. */
	au_draw_arc(c, 16.0, 48.0, 8.0, 2.0, 0.7, WHITE);
	ATF_CHECK_EQ(red(16, 40), 0);
	ATF_CHECK(red(16, 56) > 0xc0);

	/* Rounded corners leave the corner of the box. */
	au_draw_rrect(c, 36.0, 36.0, 24.0, 24.0, 8.0, RED);
	ATF_CHECK_EQ(red(48, 48), 0xff);
	ATF_CHECK_EQ(red(36, 36), 0);

	au_draw_line(c, 2.0, 62.0, 30.0, 62.0, 2.0, WHITE);
	ATF_CHECK_EQ(red(16, 62), 0xff);
	ATF_CHECK_EQ(red(16, 59), 0);
}

ATF_TC(image);
ATF_TC_HEAD(image, tc)
{
	atf_tc_set_md_var(tc, "descr", "an opaque image is copied as it is");
}
ATF_TC_BODY(image, tc)
{
	struct au_canvas *c = canvas_new();
	static uint32_t bits[4 * 4];
	pixman_image_t *src;
	int i;

	for (i = 0; i < 4 * 4; i++)
		bits[i] = 0x00102030U + (uint32_t)i;
	src = pixman_image_create_bits(PIXMAN_x8r8g8b8, 4, 4, bits, 4 * 4);
	ATF_REQUIRE(src != NULL);
	au_draw_image(c, src, 10, 10);
	ATF_CHECK_EQ(pixel(10, 10), 0x102030U);
	ATF_CHECK_EQ(pixel(13, 13), 0x102030U + 15);
	ATF_CHECK_EQ(pixel(14, 14), 0);
	pixman_image_unref(src);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, alpha);
	ATF_TP_ADD_TC(tp, fill);
	ATF_TP_ADD_TC(tp, clip);
	ATF_TP_ADD_TC(tp, shapes);
	ATF_TP_ADD_TC(tp, image);
	return atf_no_error();
}
