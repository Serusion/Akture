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

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H

#include <math.h>
#include <pixman.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "greeter.h"

/*
 * Text with FreeType.  Font files are given by path; fontconfig is
 * not used.  Variable fonts get their weight from the 'wght' axis,
 * static ones are emboldened synthetically.  Every glyph is rendered
 * once per face, size and weight and kept in a small hash table.
 */

#define GLYPH_HASHSIZE	512
#define WGHT_TAG	FT_MAKE_TAG('w', 'g', 'h', 't')
#define UTF8_BAD	0xfffd

struct face {
	FT_Face		 fa_face;
	int		 fa_wght;	/* index of the weight axis, or -1 */
	FT_UInt		 fa_naxis;
	FT_Fixed	*fa_coords;	/* design coordinates */
	FT_Fixed	 fa_wmin, fa_wmax;
	double		 fa_cursize;
	int		 fa_curweight;
};

struct glyph {
	struct glyph	*gl_next;
	const struct face *gl_face;
	uint32_t	 gl_cp;
	int		 gl_size;	/* 26.6 */
	int		 gl_weight;
	pixman_image_t	*gl_img;	/* NULL for blank glyphs */
	int		 gl_left, gl_top;
	double		 gl_advance;
};

static FT_Library font_lib;
static struct glyph *glyph_hash[GLYPH_HASHSIZE];

int
font_init(void)
{

	return FT_Init_FreeType(&font_lib) == 0 ? 0 : -1;
}

struct face *
face_open(const char *path)
{
	struct face *fa;
	FT_MM_Var *mm;
	FT_UInt i;

	if ((fa = calloc(1, sizeof(*fa))) == NULL)
		return NULL;
	if (FT_New_Face(font_lib, path, 0, &fa->fa_face) != 0) {
		free(fa);
		return NULL;
	}
	fa->fa_wght = -1;
	fa->fa_curweight = -1;
	if (FT_HAS_MULTIPLE_MASTERS(fa->fa_face) &&
	    FT_Get_MM_Var(fa->fa_face, &mm) == 0) {
		fa->fa_naxis = mm->num_axis;
		fa->fa_coords = calloc(mm->num_axis, sizeof(FT_Fixed));
		for (i = 0; fa->fa_coords != NULL && i < mm->num_axis; i++) {
			fa->fa_coords[i] = mm->axis[i].def;
			if (mm->axis[i].tag == WGHT_TAG) {
				fa->fa_wght = (int)i;
				fa->fa_wmin = mm->axis[i].minimum;
				fa->fa_wmax = mm->axis[i].maximum;
			}
		}
		FT_Done_MM_Var(font_lib, mm);
		if (fa->fa_coords == NULL)
			fa->fa_wght = -1;
	}
	return fa;
}

static void
face_setup(struct face *fa, double size, int weight)
{
	FT_Fixed w;

	if (fa->fa_cursize != size) {
		(void)FT_Set_Char_Size(fa->fa_face, 0,
		    (FT_F26Dot6)lrint(size * 64), 72, 72);
		fa->fa_cursize = size;
	}
	if (fa->fa_wght >= 0 && fa->fa_curweight != weight) {
		w = (FT_Fixed)weight << 16;
		if (w < fa->fa_wmin)
			w = fa->fa_wmin;
		if (w > fa->fa_wmax)
			w = fa->fa_wmax;
		fa->fa_coords[fa->fa_wght] = w;
		(void)FT_Set_Var_Design_Coordinates(fa->fa_face, fa->fa_naxis,
		    fa->fa_coords);
		fa->fa_curweight = weight;
	}
}

/*
 * Decode the code point at s[*i] and advance *i.  Malformed input
 * yields U+FFFD and consumes one byte.
 */
static uint32_t
utf8_next(const char *s, size_t *i)
{
	const unsigned char *p;
	uint32_t cp, min;
	int n, k;

	p = (const unsigned char *)s + *i;
	if (p[0] < 0x80) {
		(*i)++;
		return p[0];
	}
	if ((p[0] & 0xe0) == 0xc0) {
		n = 1;
		cp = p[0] & 0x1f;
		min = 0x80;
	} else if ((p[0] & 0xf0) == 0xe0) {
		n = 2;
		cp = p[0] & 0x0f;
		min = 0x800;
	} else if ((p[0] & 0xf8) == 0xf0) {
		n = 3;
		cp = p[0] & 0x07;
		min = 0x10000;
	} else {
		(*i)++;
		return UTF8_BAD;
	}
	for (k = 1; k <= n; k++) {
		if ((p[k] & 0xc0) != 0x80) {
			(*i)++;
			return UTF8_BAD;
		}
		cp = (cp << 6) | (p[k] & 0x3f);
	}
	*i += (size_t)n + 1;
	if (cp < min || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
		return UTF8_BAD;
	return cp;
}

/*
 * Offset of the character before position i of s.
 */
size_t
utf8_prev(const char *s, size_t i)
{

	while (i > 0) {
		i--;
		if (((unsigned char)s[i] & 0xc0) != 0x80)
			break;
	}
	return i;
}

/*
 * Number of characters in the first len bytes of s.
 */
size_t
utf8_len(const char *s, size_t len)
{
	size_t i, n;

	for (i = n = 0; i < len; n++)
		(void)utf8_next(s, &i);
	return n;
}

static struct glyph *
glyph_render(struct face *fa, uint32_t cp, double size, int weight)
{
	struct glyph *gl;
	FT_GlyphSlot slot;
	FT_Bitmap *bm;
	FT_UInt idx;
	uint8_t *dst;
	int y, stride;

	if ((gl = calloc(1, sizeof(*gl))) == NULL)
		return NULL;
	gl->gl_face = fa;
	gl->gl_cp = cp;
	gl->gl_size = (int)lrint(size * 64);
	gl->gl_weight = weight;

	face_setup(fa, size, weight);
	idx = FT_Get_Char_Index(fa->fa_face, cp);
	if (FT_Load_Glyph(fa->fa_face, idx,
	    FT_LOAD_NO_BITMAP | FT_LOAD_TARGET_LIGHT) != 0)
		return gl;
	slot = fa->fa_face->glyph;
	if (fa->fa_wght < 0 && weight > 500 &&
	    slot->format == FT_GLYPH_FORMAT_OUTLINE) {
		(void)FT_Outline_Embolden(&slot->outline,
		    (FT_Pos)lrint((weight - 400) / 300.0 * size * 64 / 24));
	}
	gl->gl_advance = slot->advance.x / 64.0;
	if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) != 0)
		return gl;

	bm = &slot->bitmap;
	gl->gl_left = slot->bitmap_left;
	gl->gl_top = slot->bitmap_top;
	if (bm->width == 0 || bm->rows == 0 || bm->pitch <= 0)
		return gl;
	gl->gl_img = pixman_image_create_bits(PIXMAN_a8, (int)bm->width,
	    (int)bm->rows, NULL, 0);
	if (gl->gl_img == NULL)
		return gl;
	dst = (uint8_t *)pixman_image_get_data(gl->gl_img);
	stride = pixman_image_get_stride(gl->gl_img);
	for (y = 0; y < (int)bm->rows; y++)
		(void)memcpy(dst + y * stride, bm->buffer + y * bm->pitch,
		    bm->width);
	return gl;
}

/*
 * Find the glyph for cp, taking it from the fallback face if the
 * primary one does not have it.
 */
static const struct glyph *
font_glyph(const struct font *f, uint32_t cp)
{
	struct face *fa;
	struct glyph *gl;
	unsigned int h;
	int size;

	fa = f->f_face;
	if (FT_Get_Char_Index(fa->fa_face, cp) == 0 && f->f_fallback != NULL &&
	    FT_Get_Char_Index(f->f_fallback->fa_face, cp) != 0)
		fa = f->f_fallback;

	size = (int)lrint(f->f_size * 64);
	h = (cp * 31U + (unsigned int)size + (unsigned int)f->f_weight) %
	    GLYPH_HASHSIZE;
	for (gl = glyph_hash[h]; gl != NULL; gl = gl->gl_next) {
		if (gl->gl_face == fa && gl->gl_cp == cp &&
		    gl->gl_size == size && gl->gl_weight == f->f_weight)
			return gl;
	}
	if ((gl = glyph_render(fa, cp, f->f_size, f->f_weight)) == NULL)
		return NULL;
	gl->gl_next = glyph_hash[h];
	glyph_hash[h] = gl;
	return gl;
}

double
font_width(const struct font *f, const char *s)
{
	const struct glyph *gl;
	double w;
	size_t i;

	w = 0;
	for (i = 0; s[i] != '\0';) {
		if ((gl = font_glyph(f, utf8_next(s, &i))) != NULL)
			w += gl->gl_advance;
	}
	return w;
}

double
font_ascent(const struct font *f)
{

	face_setup(f->f_face, f->f_size, f->f_weight);
	return f->f_face->fa_face->size->metrics.ascender / 64.0;
}

double
font_descent(const struct font *f)
{

	face_setup(f->f_face, f->f_size, f->f_weight);
	return -f->f_face->fa_face->size->metrics.descender / 64.0;
}

/*
 * Draw s with its baseline at y, starting at x.  Returns the x
 * coordinate after the last glyph.
 */
double
font_draw(struct canvas *c, const struct font *f, double x, double y,
    const char *s, uint32_t argb)
{
	const struct glyph *gl;
	pixman_image_t *src;
	size_t i;
	int gx, gy;

	src = draw_solid(argb);
	for (i = 0; s[i] != '\0';) {
		if ((gl = font_glyph(f, utf8_next(s, &i))) == NULL)
			continue;
		if (gl->gl_img != NULL) {
			gx = (int)lrint(x) + gl->gl_left;
			gy = (int)lrint(y) - gl->gl_top;
			pixman_image_composite32(PIXMAN_OP_OVER, src,
			    gl->gl_img, c->c_img, 0, 0, 0, 0, gx, gy,
			    pixman_image_get_width(gl->gl_img),
			    pixman_image_get_height(gl->gl_img));
		}
		x += gl->gl_advance;
	}
	pixman_image_unref(src);
	return x;
}
