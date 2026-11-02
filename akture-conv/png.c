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

#include <png.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "conv.h"

/*
 * PNG input.  Every colour type and depth ends up as 8-bit RGB;
 * alpha is composited over the background colour in linear light.
 */

static void
png_warn(png_structp png, png_const_charp msg)
{

	(void)png;
	(void)msg;
}

static void
png_fail(png_structp png, png_const_charp msg)
{

	(void)fprintf(stderr, "png: %s\n", msg);
	png_longjmp(png, 1);
}

int
png_decode(FILE *fp, struct image *im, uint32_t bg)
{
	png_structp png;
	png_infop info;
	png_bytep *volatile rows;
	uint8_t *volatile rgba;
	png_uint_32 w, h, y, x;
	float a, bl[3];
	uint8_t *s, *d;
	int k, rv;

	rows = NULL;
	rgba = NULL;
	im->im_data = NULL;
	png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, png_fail,
	    png_warn);
	if (png == NULL)
		return -1;
	if ((info = png_create_info_struct(png)) == NULL) {
		png_destroy_read_struct(&png, NULL, NULL);
		return -1;
	}
	rv = -1;
	if (setjmp(png_jmpbuf(png)))
		goto out;

	png_set_user_limits(png, CONV_DIMMAX, CONV_DIMMAX);
	png_init_io(png, fp);
	png_read_info(png, info);
	w = png_get_image_width(png, info);
	h = png_get_image_height(png, info);
	if ((uint64_t)w * h > CONV_PIXMAX) {
		(void)fprintf(stderr, "png: %ux%u is too large\n", w, h);
		goto out;
	}

	/* Normalise to 8-bit RGBA. */
	png_set_strip_16(png);
	png_set_packing(png);
	png_set_palette_to_rgb(png);
	png_set_expand_gray_1_2_4_to_8(png);
	png_set_tRNS_to_alpha(png);
	png_set_gray_to_rgb(png);
	png_set_filler(png, 0xff, PNG_FILLER_AFTER);
	(void)png_set_interlace_handling(png);
	png_read_update_info(png, info);
	if (png_get_rowbytes(png, info) != (size_t)w * 4)
		goto out;

	rgba = malloc((size_t)w * h * 4);
	rows = malloc((size_t)h * sizeof(*rows));
	if (rgba == NULL || rows == NULL || image_alloc(im, w, h) == -1)
		goto out;
	for (y = 0; y < h; y++)
		rows[y] = rgba + (size_t)y * w * 4;
	png_read_image(png, rows);
	png_read_end(png, NULL);

	bl[0] = image_linear((bg >> 16) & 0xff);
	bl[1] = image_linear((bg >> 8) & 0xff);
	bl[2] = image_linear(bg & 0xff);
	for (y = 0; y < h; y++) {
		s = rgba + (size_t)y * w * 4;
		d = im->im_data + (size_t)y * w * 3;
		for (x = 0; x < w; x++, s += 4, d += 3) {
			if (s[3] == 0xff) {
				(void)memcpy(d, s, 3);
				continue;
			}
			a = s[3] / 255.0f;
			for (k = 0; k < 3; k++)
				d[k] = image_srgb8(a * image_linear(s[k]) +
				    (1 - a) * bl[k]);
		}
	}
	rv = 0;
out:
	if (rv == -1)
		image_free(im);
	free(rows);
	free(rgba);
	png_destroy_read_struct(&png, &info, NULL);
	return rv;
}
