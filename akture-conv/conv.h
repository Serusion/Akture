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

#ifndef _CONV_H_
#define _CONV_H_

#include <stdint.h>
#include <stdio.h>

/* Limits checked before anything is decoded. */
#define CONV_DIMMAX	16384			/* per side */
#define CONV_PIXMAX	(64U * 1024 * 1024)	/* in total */

/* How an image of another aspect ratio is fitted. */
#define CONV_FILL	0	/* cover the output, crop the excess */
#define CONV_FIT	1	/* fit inside, pad with the colour */
#define CONV_CENTER	2	/* no scaling */

/* A decoded image: 8-bit sRGB, packed RGB rows. */
struct image {
	uint32_t	 im_width;
	uint32_t	 im_height;
	uint8_t		*im_data;
};

struct convopt {
	uint32_t	 co_width;
	uint32_t	 co_height;
	int		 co_mode;
	double		 co_blur;	/* radius in output pixels */
	double		 co_dark;	/* 0 .. 1 */
	uint32_t	 co_bg;		/* 0xRRGGBB */
};

/* exif.c */
int	exif_orientation(const uint8_t *, size_t);

/* image.c */
int	image_alloc(struct image *, uint32_t, uint32_t);
void	image_free(struct image *);
int	image_orient(struct image *, int);
float	image_linear(uint8_t);
uint8_t	image_srgb8(float);
int	image_render(const struct image *, const struct convopt *,
	    uint8_t *);

/* jpeg.c */
int	jpeg_decode(FILE *, struct image *);

/* png.c */
int	png_decode(FILE *, struct image *, uint32_t);

#endif /* !_CONV_H_ */
