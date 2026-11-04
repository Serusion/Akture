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

#ifndef _AUTUNI_H_
#define _AUTUNI_H_

#include <pixman.h>
#include <stddef.h>
#include <stdint.h>

/*
 * libautuni: the drawing code shared by the programs of the AutumnBSD
 * desktop.  Everything is drawn with pixman into images in memory:
 * shapes with antialiased edges, and text with FreeType.  Colours are
 * 32-bit ARGB, not premultiplied.
 *
 * The library also has the .akbg background format, in akbg.h, and the
 * parsers of the configuration, in akparse.h.  They keep their names
 * from Akture, whose master builds akparse.c itself and does not link
 * this library.
 */

struct au_rect {
	int		r_x, r_y, r_w, r_h;
};

/* draw.c: everything is drawn through a canvas clipped to the damage */
struct au_canvas {
	pixman_image_t	*c_img;
	pixman_region32_t c_clip;
};

/* font.c: a font file, opened once and drawn at any size and weight */
struct au_face;

/* font.c: a face at a size and weight, with a fallback for missing glyphs */
struct au_font {
	struct au_face	*f_face;
	struct au_face	*f_fallback;
	double		 f_size;	/* pixels */
	int		 f_weight;	/* 100 .. 900 */
};

/* The colours and the scale of a user interface. */
struct au_theme {
	uint32_t	 th_bgcolor;	/* background */
	uint32_t	 th_accent;
	uint32_t	 th_text;
	double		 th_scale;	/* factor for all sizes */
};

/* draw.c */
/* A solid fill of the colour, as a source image to unref when done. */
pixman_image_t *au_draw_solid(uint32_t);
/* The colour with its alpha replaced. */
uint32_t au_draw_alpha(uint32_t, unsigned int);
/* Fill a rectangle; an opaque colour replaces what was there. */
void	au_draw_fill(struct au_canvas *, const struct au_rect *, uint32_t);
/* Copy an opaque image, e.g. a background, with its corner at (x, y). */
void	au_draw_image(struct au_canvas *, pixman_image_t *, int, int);
/* Fill a rectangle (x, y, w, h) with corners of radius r. */
void	au_draw_rrect(struct au_canvas *, double, double, double, double,
	    double, uint32_t);
/* Outline the same rectangle with a line of width lw. */
void	au_draw_rrect_stroke(struct au_canvas *, double, double, double,
	    double, double, double, uint32_t);
/* Fill a circle of radius r around (cx, cy). */
void	au_draw_circle(struct au_canvas *, double, double, double, uint32_t);
/* Outline a circle of radius r around (cx, cy), lw wide. */
void	au_draw_ring(struct au_canvas *, double, double, double, double,
	    uint32_t);
/* Draw a line from (x0, y0) to (x1, y1), lw wide, with round ends. */
void	au_draw_line(struct au_canvas *, double, double, double, double,
	    double, uint32_t);
/* Draw a ring open at the top by gap radians each side, as for power. */
void	au_draw_arc(struct au_canvas *, double, double, double, double, double,
	    uint32_t);

/* font.c */
/* Start FreeType, before any face is opened; 0, or -1 on failure. */
int	au_font_init(void);
/* Open the font file at path; NULL if it cannot be used. */
struct au_face *au_face_open(const char *);
/* The width of a UTF-8 string, in pixels. */
double	au_font_width(const struct au_font *, const char *);
/* The height of the font above the baseline, in pixels. */
double	au_font_ascent(const struct au_font *);
/* The depth of the font below the baseline, in pixels. */
double	au_font_descent(const struct au_font *);
/* Draw a UTF-8 string from (x, baseline y); returns the x after it. */
double	au_font_draw(struct au_canvas *, const struct au_font *, double,
	    double, const char *, uint32_t);
/* The offset of the character before offset i of s. */
size_t	au_utf8_prev(const char *, size_t);
/* The number of characters in the first len bytes of s. */
size_t	au_utf8_len(const char *, size_t);

#endif /* !_AUTUNI_H_ */
