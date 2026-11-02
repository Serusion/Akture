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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "conv.h"

/*
 * Image processing.  All filtering happens on linear light: sRGB
 * values are linearised, scaled, blurred and darkened, and encoded
 * back to sRGB at the very end, so that reductions do not turn dark
 * and muddy.
 */

#define CUBIC_SUPPORT	2.0	/* Catmull-Rom */

/* One output sample of a separable filter. */
struct contrib {
	int		 c_first;	/* first source index */
	int		 c_n;
	float		*c_w;		/* c_n weights summing to 1 */
};

static float srgb_to_linear[256];

int
image_alloc(struct image *im, uint32_t w, uint32_t h)
{

	im->im_width = w;
	im->im_height = h;
	im->im_data = malloc((size_t)w * h * 3);
	return im->im_data != NULL ? 0 : -1;
}

void
image_free(struct image *im)
{

	free(im->im_data);
	im->im_data = NULL;
}

float
image_linear(uint8_t v)
{
	double c;
	int i;

	if (srgb_to_linear[255] == 0) {
		for (i = 0; i < 256; i++) {
			c = i / 255.0;
			srgb_to_linear[i] = (float)(c <= 0.04045 ? c / 12.92 :
			    pow((c + 0.055) / 1.055, 2.4));
		}
	}
	return srgb_to_linear[v];
}

static float
image_srgb(float l)
{

	if (l <= 0.0f)
		return 0.0f;
	if (l >= 1.0f)
		return 1.0f;
	return l <= 0.0031308f ? l * 12.92f :
	    1.055f * powf(l, 1.0f / 2.4f) - 0.055f;
}

uint8_t
image_srgb8(float l)
{

	return (uint8_t)lrintf(image_srgb(l) * 255.0f);
}

/*
 * Apply an EXIF orientation (1 .. 8) so that the image is upright.
 */
int
image_orient(struct image *im, int o)
{
	struct image out;
	uint32_t w, h, x, y, sx, sy;

	if (o <= 1 || o > 8)
		return 0;
	w = im->im_width;
	h = im->im_height;
	if (o >= 5 ? image_alloc(&out, h, w) : image_alloc(&out, w, h))
		return -1;
	for (y = 0; y < out.im_height; y++) {
		for (x = 0; x < out.im_width; x++) {
			switch (o) {
			case 2: sx = w - 1 - x; sy = y; break;
			case 3: sx = w - 1 - x; sy = h - 1 - y; break;
			case 4: sx = x; sy = h - 1 - y; break;
			case 5: sx = y; sy = x; break;
			case 6: sx = y; sy = h - 1 - x; break;
			case 7: sx = w - 1 - y; sy = h - 1 - x; break;
			default: sx = w - 1 - y; sy = x; break;
			}
			(void)memcpy(out.im_data + ((size_t)y * out.im_width + x)
			    * 3, im->im_data + ((size_t)sy * w + sx) * 3, 3);
		}
	}
	image_free(im);
	*im = out;
	return 0;
}

static double
cubic(double x)
{

	x = fabs(x);
	if (x < 1)
		return 1.5 * x * x * x - 2.5 * x * x + 1;
	if (x < 2)
		return -0.5 * x * x * x + 2.5 * x * x - 4 * x + 2;
	return 0;
}

/*
 * Weights for n output samples of a source scaled by scale and shifted
 * by off (the output coordinate of the left edge of the source).
 * When reducing, the kernel is widened so that it also averages.
 */
static void	contrib_free(struct contrib *, int);

static struct contrib *
contrib_make(int n, double scale, double off)
{
	struct contrib *c;
	double centre, support, fscale, sum, w;
	int i, j, first, last;

	fscale = scale < 1 ? scale : 1;
	support = CUBIC_SUPPORT / fscale;
	if ((c = calloc((size_t)n, sizeof(*c))) == NULL)
		return NULL;
	for (i = 0; i < n; i++) {
		centre = (i + 0.5 - off) / scale - 0.5;
		first = (int)ceil(centre - support);
		last = (int)floor(centre + support);
		c[i].c_first = first;
		c[i].c_n = last - first + 1;
		if ((c[i].c_w = calloc((size_t)c[i].c_n, sizeof(float))) ==
		    NULL) {
			contrib_free(c, i);
			return NULL;
		}
		sum = 0;
		for (j = 0; j < c[i].c_n; j++) {
			w = cubic((first + j - centre) * fscale);
			c[i].c_w[j] = (float)w;
			sum += w;
		}
		for (j = 0; j < c[i].c_n && sum != 0; j++)
			c[i].c_w[j] = (float)(c[i].c_w[j] / sum);
	}
	return c;
}

static void
contrib_free(struct contrib *c, int n)
{
	int i;

	if (c == NULL)
		return;
	for (i = 0; i < n; i++)
		free(c[i].c_w);
	free(c);
}

static int
clampi(int v, int lo, int hi)
{

	return v < lo ? lo : v > hi ? hi : v;
}

/*
 * Three box blurs approximate a gaussian (Kovesi's box sizes).
 */
static void
box_pass(float *src, float *dst, int w, int h, int r, int horizontal)
{
	double acc[3];
	float inv;
	int i, j, k, n, len, stride, lo, hi;

	inv = 1.0f / (2 * r + 1);
	len = horizontal ? w : h;
	n = horizontal ? h : w;
	stride = horizontal ? 3 : w * 3;
	for (i = 0; i < n; i++) {
		float *s = horizontal ? src + (size_t)i * w * 3 : src + i * 3;
		float *d = horizontal ? dst + (size_t)i * w * 3 : dst + i * 3;

		acc[0] = acc[1] = acc[2] = 0;
		for (j = -r; j <= r; j++) {
			for (k = 0; k < 3; k++)
				acc[k] += s[clampi(j, 0, len - 1) * stride + k];
		}
		for (j = 0; j < len; j++) {
			for (k = 0; k < 3; k++)
				d[j * stride + k] = (float)(acc[k] * inv);
			lo = clampi(j - r, 0, len - 1);
			hi = clampi(j + r + 1, 0, len - 1);
			for (k = 0; k < 3; k++)
				acc[k] += s[hi * stride + k] - s[lo * stride + k];
		}
	}
}

static int
blur(float *img, int w, int h, double radius)
{
	double sigma, wideal;
	float *tmp;
	int sizes[3], wl, m, i;

	sigma = radius / 2;
	wideal = sqrt(12 * sigma * sigma / 3 + 1);
	wl = (int)floor(wideal);
	if (wl % 2 == 0)
		wl--;
	m = (int)lrint((12 * sigma * sigma - 3.0 * wl * wl - 12.0 * wl - 9) /
	    (-4.0 * wl - 4));
	for (i = 0; i < 3; i++)
		sizes[i] = i < m ? wl : wl + 2;

	if ((tmp = malloc((size_t)w * h * 3 * sizeof(float))) == NULL)
		return -1;
	for (i = 0; i < 3; i++) {
		if (sizes[i] < 3)
			continue;
		box_pass(img, tmp, w, h, (sizes[i] - 1) / 2, 1);
		box_pass(tmp, img, w, h, (sizes[i] - 1) / 2, 0);
	}
	free(tmp);
	return 0;
}

/*
 * Darkening, with the faint shadows at the top and bottom of plan D:
 * the full amount at the very edges, 40% of it in the middle.
 */
static float
shade(int y, int h)
{
	double t, top, bottom, edge;

	t = h > 1 ? (double)y / (h - 1) : 0;
	top = fmax(0, 1 - t / 0.25);
	bottom = fmax(0, 1 - (1 - t) / 0.35);
	edge = fmax(top * top, bottom * bottom);
	return (float)(0.4 + 0.6 * edge);
}

/*
 * Render the image into out as BGRX rows of the requested size.
 */
int
image_render(const struct image *im, const struct convopt *co, uint8_t *out)
{
	static const float bayer[4][4] = {
		{  0,  8,  2, 10 }, { 12,  4, 14,  6 },
		{  3, 11,  1,  9 }, { 15,  7, 13,  5 },
	};
	struct contrib *cx, *cy;
	float *tmp, *lin, *p, bg[3], v, f;
	double scale, sw, sh, ox, oy;
	int W, H, w, h, x, y, j, k, sx, sy, x0, x1, y0, y1, rv;

	W = (int)co->co_width;
	H = (int)co->co_height;
	w = (int)im->im_width;
	h = (int)im->im_height;
	switch (co->co_mode) {
	case CONV_FIT:
		scale = fmin((double)W / w, (double)H / h);
		break;
	case CONV_CENTER:
		scale = 1;
		break;
	default:
		scale = fmax((double)W / w, (double)H / h);
		break;
	}
	sw = w * scale;
	sh = h * scale;
	ox = (W - sw) / 2;
	oy = (H - sh) / 2;
	/* Output pixels whose centre falls on the scaled image. */
	x0 = clampi((int)ceil(ox - 0.5), 0, W);
	x1 = clampi((int)ceil(ox + sw - 0.5), 0, W);
	y0 = clampi((int)ceil(oy - 0.5), 0, H);
	y1 = clampi((int)ceil(oy + sh - 0.5), 0, H);

	bg[0] = image_linear((co->co_bg >> 16) & 0xff);
	bg[1] = image_linear((co->co_bg >> 8) & 0xff);
	bg[2] = image_linear(co->co_bg & 0xff);

	rv = -1;
	tmp = lin = NULL;
	cx = contrib_make(W, scale, ox);
	cy = contrib_make(H, scale, oy);
	tmp = malloc((size_t)W * h * 3 * sizeof(float));
	lin = malloc((size_t)W * H * 3 * sizeof(float));
	if (cx == NULL || cy == NULL || tmp == NULL || lin == NULL)
		goto out;

	/* Horizontal pass, source rows to output columns. */
	for (y = 0; y < h; y++) {
		const uint8_t *row = im->im_data + (size_t)y * w * 3;

		for (x = x0; x < x1; x++) {
			p = tmp + ((size_t)y * W + x) * 3;
			p[0] = p[1] = p[2] = 0;
			for (j = 0; j < cx[x].c_n; j++) {
				sx = clampi(cx[x].c_first + j, 0, w - 1);
				for (k = 0; k < 3; k++)
					p[k] += cx[x].c_w[j] *
					    image_linear(row[sx * 3 + k]);
			}
		}
	}
	/* Vertical pass; the rest is background. */
	for (y = 0; y < H; y++) {
		for (x = 0; x < W; x++) {
			p = lin + ((size_t)y * W + x) * 3;
			if (y < y0 || y >= y1 || x < x0 || x >= x1) {
				(void)memcpy(p, bg, sizeof(bg));
				continue;
			}
			p[0] = p[1] = p[2] = 0;
			for (j = 0; j < cy[y].c_n; j++) {
				sy = clampi(cy[y].c_first + j, 0, h - 1);
				for (k = 0; k < 3; k++)
					p[k] += cy[y].c_w[j] *
					    tmp[((size_t)sy * W + x) * 3 + k];
			}
		}
	}

	if (co->co_blur > 0 && blur(lin, W, H, co->co_blur) == -1)
		goto out;

	for (y = 0; y < H; y++) {
		f = 1.0f - (float)co->co_dark * shade(y, H);
		for (x = 0; x < W; x++) {
			p = lin + ((size_t)y * W + x) * 3;
			for (k = 0; k < 3; k++) {
				/* Ordered dither hides banding in gradients. */
				v = image_srgb(p[k] * f) * 255.0f +
				    (bayer[y & 3][x & 3] - 7.5f) / 16.0f;
				v = v < 0 ? 0 : v > 255 ? 255 : v;
				/* BGRX: blue first */
				out[((size_t)y * W + x) * 4 + 2 - k] =
				    (uint8_t)lrintf(v);
			}
			out[((size_t)y * W + x) * 4 + 3] = 0;
		}
	}
	rv = 0;
out:
	contrib_free(cx, W);
	contrib_free(cy, H);
	free(tmp);
	free(lin);
	return rv;
}
