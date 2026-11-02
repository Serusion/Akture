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

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jpeglib.h>

#include "conv.h"

/*
 * JPEG input with libjpeg(-turbo).  Grey and YCbCr images come out as
 * RGB; CMYK (as written by Adobe, inverted) is converted here.  The
 * EXIF orientation is applied so that the image is upright.
 */

#define JPEG_APP1	(JPEG_APP0 + 1)

struct jpeg_err {
	struct jpeg_error_mgr	je_pub;
	jmp_buf			je_jmp;
};

static void
jpeg_fail(j_common_ptr cinfo)
{
	struct jpeg_err *err = (struct jpeg_err *)cinfo->err;

	(*cinfo->err->output_message)(cinfo);
	longjmp(err->je_jmp, 1);
}

static int
jpeg_orientation(j_decompress_ptr cinfo)
{
	jpeg_saved_marker_ptr m;
	int o;

	for (m = cinfo->marker_list; m != NULL; m = m->next) {
		if (m->marker == JPEG_APP1 &&
		    (o = exif_orientation(m->data, m->data_length)) != 1)
			return o;
	}
	return 1;
}

int
jpeg_decode(FILE *fp, struct image *im)
{
	struct jpeg_decompress_struct cinfo;
	struct jpeg_err err;
	JSAMPROW row;
	uint8_t *volatile cmyk;
	uint8_t *d;
	JDIMENSION x;
	int volatile orientation;
	unsigned int k;

	cmyk = NULL;
	im->im_data = NULL;
	orientation = 1;
	cinfo.err = jpeg_std_error(&err.je_pub);
	err.je_pub.error_exit = jpeg_fail;
	if (setjmp(err.je_jmp)) {
		jpeg_destroy_decompress(&cinfo);
		image_free(im);
		free(cmyk);
		return -1;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_save_markers(&cinfo, JPEG_APP1, 0xffff);
	jpeg_stdio_src(&cinfo, fp);
	(void)jpeg_read_header(&cinfo, TRUE);
	if (cinfo.image_width > CONV_DIMMAX ||
	    cinfo.image_height > CONV_DIMMAX ||
	    (uint64_t)cinfo.image_width * cinfo.image_height > CONV_PIXMAX) {
		(void)fprintf(stderr, "jpeg: %ux%u is too large\n",
		    cinfo.image_width, cinfo.image_height);
		jpeg_destroy_decompress(&cinfo);
		return -1;
	}
	orientation = jpeg_orientation(&cinfo);

	if (cinfo.jpeg_color_space == JCS_CMYK ||
	    cinfo.jpeg_color_space == JCS_YCCK)
		cinfo.out_color_space = JCS_CMYK;
	else
		cinfo.out_color_space = JCS_RGB;
	(void)jpeg_start_decompress(&cinfo);
	if (image_alloc(im, cinfo.output_width, cinfo.output_height) == -1)
		longjmp(err.je_jmp, 1);
	if (cinfo.out_color_space == JCS_CMYK &&
	    (cmyk = malloc((size_t)cinfo.output_width * 4)) == NULL)
		longjmp(err.je_jmp, 1);

	while (cinfo.output_scanline < cinfo.output_height) {
		d = im->im_data +
		    (size_t)cinfo.output_scanline * cinfo.output_width * 3;
		row = cmyk != NULL ? cmyk : d;
		(void)jpeg_read_scanlines(&cinfo, &row, 1);
		if (cmyk == NULL)
			continue;
		for (x = 0; x < cinfo.output_width; x++) {
			for (k = 0; k < 3; k++)
				d[x * 3 + k] = (uint8_t)(cmyk[x * 4 + k] *
				    cmyk[x * 4 + 3] / 255);
		}
	}
	(void)jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	free(cmyk);

	return image_orient(im, orientation);
}
