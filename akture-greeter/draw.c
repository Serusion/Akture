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

#include <math.h>
#include <pixman.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "autuni.h"

/*
 * Drawing primitives.  Shapes are rasterised from signed distance
 * functions into an 8-bit coverage mask, which pixman composites onto
 * the frame through the canvas clip; this gives antialiased edges
 * without any help from the X server.
 */

#define SHAPE_RRECT		0	/* filled rounded rectangle */
#define SHAPE_RRECT_STROKE	1
#define SHAPE_SEGMENT		2	/* line with round caps */
#define SHAPE_ARC		3	/* ring open at the top */

struct shape {
	int		sh_kind;
	double		sh_x0, sh_y0;	/* centre, or first end */
	double		sh_x1, sh_y1;	/* half size, or second end */
	double		sh_r;		/* corner or arc radius */
	double		sh_lw;		/* line width */
	double		sh_gap;		/* half opening of an arc, radians */
};

pixman_image_t *
au_draw_solid(uint32_t argb)
{
	pixman_color_t c;
	unsigned int a;

	a = argb >> 24;
	c.alpha = (uint16_t)(a * 257);
	c.red = (uint16_t)(((argb >> 16) & 0xff) * a * 257 / 255);
	c.green = (uint16_t)(((argb >> 8) & 0xff) * a * 257 / 255);
	c.blue = (uint16_t)((argb & 0xff) * a * 257 / 255);
	return pixman_image_create_solid_fill(&c);
}

/*
 * Replace the alpha of a colour.
 */
uint32_t
au_draw_alpha(uint32_t argb, unsigned int alpha)
{

	return (argb & 0x00ffffffU) | ((uint32_t)(alpha & 0xff) << 24);
}

static int
draw_visible(const struct au_canvas *c, int x, int y, int w, int h)
{
	pixman_box32_t box;

	if (w <= 0 || h <= 0)
		return 0;
	box.x1 = x;
	box.y1 = y;
	box.x2 = x + w;
	box.y2 = y + h;
	return pixman_region32_contains_rectangle(
	    __UNCONST(&c->c_clip), &box) != PIXMAN_REGION_OUT;
}

void
au_draw_fill(struct au_canvas *c, const struct au_rect *r, uint32_t argb)
{
	pixman_image_t *src;

	if (!draw_visible(c, r->r_x, r->r_y, r->r_w, r->r_h))
		return;
	src = au_draw_solid(argb);
	pixman_image_composite32((argb >> 24) == 0xff ? PIXMAN_OP_SRC :
	    PIXMAN_OP_OVER, src, NULL, c->c_img, 0, 0, 0, 0,
	    r->r_x, r->r_y, r->r_w, r->r_h);
	pixman_image_unref(src);
}

/*
 * Copy an opaque image, e.g. a background, to (x, y).
 */
void
au_draw_image(struct au_canvas *c, pixman_image_t *img, int x, int y)
{
	int w, h;

	w = pixman_image_get_width(img);
	h = pixman_image_get_height(img);
	if (!draw_visible(c, x, y, w, h))
		return;
	pixman_image_composite32(PIXMAN_OP_SRC, img, NULL, c->c_img,
	    0, 0, 0, 0, x, y, w, h);
}

static double
sdf_rrect(const struct shape *s, double px, double py)
{
	double qx, qy, ox, oy;

	qx = fabs(px - s->sh_x0) - (s->sh_x1 - s->sh_r);
	qy = fabs(py - s->sh_y0) - (s->sh_y1 - s->sh_r);
	ox = fmax(qx, 0.0);
	oy = fmax(qy, 0.0);
	return sqrt(ox * ox + oy * oy) + fmin(fmax(qx, qy), 0.0) - s->sh_r;
}

static double
sdf_segment(double px, double py, double ax, double ay, double bx,
    double by)
{
	double dx, dy, t, len2;

	dx = bx - ax;
	dy = by - ay;
	len2 = dx * dx + dy * dy;
	t = len2 > 0 ? ((px - ax) * dx + (py - ay) * dy) / len2 : 0;
	t = fmin(fmax(t, 0.0), 1.0);
	return hypot(px - (ax + t * dx), py - (ay + t * dy));
}

static double
sdf_arc(const struct shape *s, double px, double py)
{
	double dx, dy, ang, ex, ey;

	dx = px - s->sh_x0;
	dy = py - s->sh_y0;
	ang = atan2(dx, -dy);		/* 0 at the top, clockwise */
	if (fabs(ang) >= s->sh_gap)
		return fabs(hypot(dx, dy) - s->sh_r) - s->sh_lw / 2;
	/* Inside the opening: distance to the nearer round cap. */
	ex = s->sh_r * sin(s->sh_gap);
	ey = -s->sh_r * cos(s->sh_gap);
	return fmin(hypot(dx - ex, dy - ey), hypot(dx + ex, dy - ey)) -
	    s->sh_lw / 2;
}

static double
sdf(const struct shape *s, double px, double py)
{

	switch (s->sh_kind) {
	case SHAPE_RRECT:
		return sdf_rrect(s, px, py);
	case SHAPE_RRECT_STROKE:
		return fabs(sdf_rrect(s, px, py)) - s->sh_lw / 2;
	case SHAPE_SEGMENT:
		return sdf_segment(px, py, s->sh_x0, s->sh_y0, s->sh_x1,
		    s->sh_y1) - s->sh_lw / 2;
	case SHAPE_ARC:
		return sdf_arc(s, px, py);
	default:
		return 1.0;
	}
}

/*
 * Rasterise a shape whose ink lies within the given box.
 */
static void
draw_shape(struct au_canvas *c, const struct shape *s, double bx0, double by0,
    double bx1, double by1, uint32_t argb)
{
	pixman_image_t *mask, *src;
	uint8_t *cov;
	double d;
	int x0, y0, w, h, stride, x, y;

	x0 = (int)floor(bx0) - 1;
	y0 = (int)floor(by0) - 1;
	w = (int)ceil(bx1) + 1 - x0;
	h = (int)ceil(by1) + 1 - y0;
	if (!draw_visible(c, x0, y0, w, h))
		return;

	stride = (w + 3) & ~3;
	if ((cov = calloc((size_t)stride, (size_t)h)) == NULL)
		return;
	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			d = sdf(s, x0 + x + 0.5, y0 + y + 0.5);
			if (d < 0.5)
				cov[y * stride + x] = (uint8_t)lrint(
				    fmin(0.5 - d, 1.0) * 255);
		}
	}

	mask = pixman_image_create_bits(PIXMAN_a8, w, h, (uint32_t *)cov,
	    stride);
	src = au_draw_solid(argb);
	pixman_image_composite32(PIXMAN_OP_OVER, src, mask, c->c_img,
	    0, 0, 0, 0, x0, y0, w, h);
	pixman_image_unref(src);
	pixman_image_unref(mask);
	free(cov);
}

void
au_draw_rrect(struct au_canvas *c, double x, double y, double w, double h,
    double r, uint32_t argb)
{
	struct shape s;

	(void)memset(&s, 0, sizeof(s));
	s.sh_kind = SHAPE_RRECT;
	s.sh_x0 = x + w / 2;
	s.sh_y0 = y + h / 2;
	s.sh_x1 = w / 2;
	s.sh_y1 = h / 2;
	s.sh_r = fmin(r, fmin(w, h) / 2);
	draw_shape(c, &s, x, y, x + w, y + h, argb);
}

void
au_draw_rrect_stroke(struct au_canvas *c, double x, double y, double w,
    double h, double r, double lw, uint32_t argb)
{
	struct shape s;

	(void)memset(&s, 0, sizeof(s));
	s.sh_kind = SHAPE_RRECT_STROKE;
	s.sh_x0 = x + w / 2;
	s.sh_y0 = y + h / 2;
	s.sh_x1 = w / 2;
	s.sh_y1 = h / 2;
	s.sh_r = fmin(r, fmin(w, h) / 2);
	s.sh_lw = lw;
	draw_shape(c, &s, x - lw, y - lw, x + w + lw, y + h + lw, argb);
}

void
au_draw_circle(struct au_canvas *c, double cx, double cy, double r,
    uint32_t argb)
{

	au_draw_rrect(c, cx - r, cy - r, 2 * r, 2 * r, r, argb);
}

void
au_draw_ring(struct au_canvas *c, double cx, double cy, double r, double lw,
    uint32_t argb)
{

	au_draw_rrect_stroke(c, cx - r, cy - r, 2 * r, 2 * r, r, lw, argb);
}

void
au_draw_line(struct au_canvas *c, double x0, double y0, double x1, double y1,
    double lw, uint32_t argb)
{
	struct shape s;

	(void)memset(&s, 0, sizeof(s));
	s.sh_kind = SHAPE_SEGMENT;
	s.sh_x0 = x0;
	s.sh_y0 = y0;
	s.sh_x1 = x1;
	s.sh_y1 = y1;
	s.sh_lw = lw;
	draw_shape(c, &s, fmin(x0, x1) - lw, fmin(y0, y1) - lw,
	    fmax(x0, x1) + lw, fmax(y0, y1) + lw, argb);
}

/*
 * A ring of radius r open at the top by gap radians on either side,
 * as in the usual power symbol.
 */
void
au_draw_arc(struct au_canvas *c, double cx, double cy, double r, double lw,
    double gap, uint32_t argb)
{
	struct shape s;

	(void)memset(&s, 0, sizeof(s));
	s.sh_kind = SHAPE_ARC;
	s.sh_x0 = cx;
	s.sh_y0 = cy;
	s.sh_r = r;
	s.sh_lw = lw;
	s.sh_gap = gap;
	draw_shape(c, &s, cx - r - lw, cy - r - lw, cx + r + lw, cy + r + lw,
	    argb);
}
