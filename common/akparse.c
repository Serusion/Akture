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

#include "akparse.h"

/*
 * Lexical helpers shared by the configuration reader of the master,
 * the session list and the greeter.  None of them allocates memory or
 * calls into the system, so Frama-C/Eva can show the absence of
 * run-time errors and the fuzzer can drive them directly.
 */

/*@ assigns \nothing; */
static int
isspc(char c)
{

	return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

/*@ assigns \nothing; */
static int
iskeychar(char c)
{

	return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
}

/*@ assigns \nothing; */
static int
hexval(char c)
{

	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/*
 * Copy line[from, to) into dst, which has room for AKPARSE_LINEMAX
 * bytes and is all zeros, so the copy stays NUL terminated.
 */
/*@
  requires \valid(dst + (0 .. AKPARSE_LINEMAX - 1));
  assigns dst[0 .. AKPARSE_LINEMAX - 2];
*/
static void
ak_copy(char *dst, const char *line, size_t from, size_t to)
{
	size_t i, n;

	/*@
	  loop invariant 0 <= n <= AKPARSE_LINEMAX - 1;
	  loop assigns i, n, dst[0 .. AKPARSE_LINEMAX - 2];
	  loop variant AKPARSE_LINEMAX - 1 - n;
	*/
	for (i = from, n = 0; i < to && n < AKPARSE_LINEMAX - 1; i++, n++)
		dst[n] = line[i];
}

/*
 * Split a "key = value" line into key and val, which must have room
 * for AKPARSE_LINEMAX bytes each.  Only lines whose first non-blank
 * character is '#' are comments: a '#' later in the line belongs to
 * the value, because colours are written as #RRGGBB.
 *
 * Every index is checked against the length rather than just for the
 * NUL, and the parts are copied to buffers of their own; this keeps
 * the bounds evident to Frama-C/Eva.
 */
/*@
  requires valid_read_string(line);
  requires \valid(key + (0 .. AKPARSE_LINEMAX - 1));
  requires \valid(val + (0 .. AKPARSE_LINEMAX - 1));
  requires \separated(key + (0 .. AKPARSE_LINEMAX - 1),
    val + (0 .. AKPARSE_LINEMAX - 1), line + (0 .. strlen(line)));
  assigns key[0 .. AKPARSE_LINEMAX - 1], val[0 .. AKPARSE_LINEMAX - 1];
  ensures \result == AKPARSE_EMPTY || \result == AKPARSE_PAIR ||
    \result == AKPARSE_ERROR;
*/
int
ak_parse_line(const char *line, char *key, char *val)
{
	size_t len, i, k, kend, v, vend;

	len = strnlen(line, AKPARSE_LINEMAX - 1);
	(void)memset(key, 0, AKPARSE_LINEMAX);
	(void)memset(val, 0, AKPARSE_LINEMAX);
	/*@
	  loop invariant 0 <= i <= len;
	  loop assigns i;
	  loop variant len - i;
	*/
	for (i = 0; i < len && isspc(line[i]); i++)
		continue;
	if (i >= len || line[i] == '#')
		return AKPARSE_EMPTY;

	k = i;
	/*@
	  loop invariant k <= i <= len;
	  loop assigns i;
	  loop variant len - i;
	*/
	while (i < len && iskeychar(line[i]))
		i++;
	if (i == k)
		return AKPARSE_ERROR;
	kend = i;
	/*@
	  loop invariant kend <= i <= len;
	  loop assigns i;
	  loop variant len - i;
	*/
	while (i < len && (line[i] == ' ' || line[i] == '\t'))
		i++;
	if (i >= len || line[i] != '=')
		return AKPARSE_ERROR;

	/*@
	  loop invariant 0 < i <= len;
	  loop assigns i;
	  loop variant len - i;
	*/
	for (i++; i < len && (line[i] == ' ' || line[i] == '\t'); i++)
		continue;
	v = i;
	vend = len;
	/*@
	  loop invariant v <= vend <= len;
	  loop assigns vend;
	  loop variant vend - v;
	*/
	while (vend > v && isspc(line[vend - 1]))
		vend--;

	ak_copy(key, line, k, kend);
	ak_copy(val, line, v, vend);
	return AKPARSE_PAIR;
}

/*
 * Split s in place into at most max-1 blank separated words and
 * terminate argv with NULL.  No quoting: the result goes to execve(2)
 * and never through a shell.  Returns the number of words, or -1 if
 * there are too many.
 */
/*@
  requires valid_string(s);
  requires max >= 1 && \valid(argv + (0 .. max - 1));
  assigns s[0 .. strlen(s)], argv[0 .. max - 1];
  ensures -1 <= \result < max;
*/
int
ak_split_words(char *s, char **argv, int max)
{
	size_t len, i;
	int n;

	len = strlen(s);
	n = 0;
	i = 0;
	/*@
	  loop invariant 0 <= n < max;
	  loop invariant 0 <= i <= len;
	  loop assigns i, n, s[0 .. len - 1], argv[0 .. max - 2];
	  loop variant len - i;
	*/
	for (;;) {
		/*@
		  loop invariant \at(i, LoopEntry) <= i <= len;
		  loop assigns i;
		  loop variant len - i;
		*/
		while (i < len && isspc(s[i]))
			i++;
		if (i >= len)
			break;
		if (n >= max - 1)
			return -1;
		/* s[i] starts a word. */
		argv[n++] = s + i;
		i++;
		/*@
		  loop invariant \at(i, LoopEntry) <= i <= len;
		  loop assigns i;
		  loop variant len - i;
		*/
		while (i < len && !isspc(s[i]))
			i++;
		if (i < len)
			s[i++] = '\0';
	}
	argv[n] = NULL;
	return n;
}

/*
 * Parse "#RRGGBB" into an opaque 0xAARRGGBB value.
 */
/*@
  requires valid_read_string(s);
  requires \valid(argb);
  assigns *argb;
*/
int
ak_parse_color(const char *s, uint32_t *argb)
{
	uint32_t v;
	int i, d;

	if (strlen(s) != 7 || s[0] != '#')
		return -1;
	v = 0;
	/*@
	  loop invariant 1 <= i <= 7;
	  loop assigns i, d, v;
	  loop variant 7 - i;
	*/
	for (i = 1; i < 7; i++) {
		if ((d = hexval(s[i])) < 0)
			return -1;
		v = (v << 4) | (uint32_t)d;
	}
	*argb = 0xff000000U | v;
	return 0;
}

/*@
  requires valid_read_string(s);
  requires \valid(b);
  assigns *b;
*/
int
ak_parse_bool(const char *s, int *b)
{

	if (strcmp(s, "yes") == 0 || strcmp(s, "true") == 0 ||
	    strcmp(s, "1") == 0) {
		*b = 1;
		return 0;
	}
	if (strcmp(s, "no") == 0 || strcmp(s, "false") == 0 ||
	    strcmp(s, "0") == 0) {
		*b = 0;
		return 0;
	}
	return -1;
}

/*
 * The scale factor is one of a few fixed values; it is returned in
 * percent so that no floating point is needed by the master.
 */
/*@
  requires valid_read_string(s);
  requires \valid(pct);
  assigns *pct;
  ensures \result == 0 ==>
    *pct == 100 || *pct == 125 || *pct == 150 || *pct == 200;
*/
int
ak_parse_scale(const char *s, unsigned int *pct)
{
	static const struct {
		const char	*name;
		unsigned int	 pct;
	} scales[] = {
		{ "1", 100 }, { "1.0", 100 },
		{ "1.25", 125 },
		{ "1.5", 150 }, { "1.50", 150 },
		{ "2", 200 }, { "2.0", 200 },
	};
	size_t i;

	/*@
	  loop invariant 0 <= i <= sizeof(scales) / sizeof(scales[0]);
	  loop assigns i;
	  loop variant sizeof(scales) / sizeof(scales[0]) - i;
	*/
	for (i = 0; i < sizeof(scales) / sizeof(scales[0]); i++) {
		if (strcmp(s, scales[i].name) == 0) {
			*pct = scales[i].pct;
			return 0;
		}
	}
	return -1;
}
