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
#include <sys/endian.h>

#include <stdint.h>
#include <string.h>

#include "akmsg.h"
#include "akutil.h"

/* Flags of a string field. */
#define ML_TEXT		0x01	/* may contain newlines and tabs */

struct akmsg_layout {
	uint32_t	ml_type;
	uint32_t	ml_nargs;
	uint32_t	ml_argmin;	/* range of the first argument */
	uint32_t	ml_argmax;
	uint32_t	ml_strmax;	/* 0: the type carries no string */
	uint32_t	ml_flags;
};

static const struct akmsg_layout akmsg_layouts[] = {
	{ AKMSG_BEGIN_AUTH,   1, 0, AK_MAXSESSIONS - 1, AKMSG_NAMEMAX, 0 },
	{ AKMSG_RESPONSE,     0, 0, 0, AKMSG_RESPMAX, 0 },
	{ AKMSG_CANCEL,       0, 0, 0, 0, 0 },
	{ AKMSG_POWER,        1, AKPOWER_SHUTDOWN, AKPOWER_REBOOT, 0, 0 },
	{ AKMSG_INIT_OPTION,  1, 0, UINT32_MAX, AKMSG_TEXTMAX, 0 },
	{ AKMSG_INIT_SESSION, 1, 0, AK_MAXSESSIONS - 1, AKMSG_NAMEMAX, 0 },
	{ AKMSG_INIT,         3, 0, AKINIT_ALL, AKMSG_NAMEMAX, 0 },
	{ AKMSG_PROMPT,       1, AKPROMPT_SECRET, AKPROMPT_ERROR,
	  AKMSG_TEXTMAX, ML_TEXT },
	{ AKMSG_AUTH_RESULT,  1, AKRESULT_FAIL, AKRESULT_OK, 0, 0 },
};

#define AKMSG_NLAYOUTS	__arraycount(akmsg_layouts)

/*@
  predicate cleared{L}(struct akmsg *m) =
    m->am_type == AKMSG_NONE && m->am_seq == 0 && m->am_len == 0 &&
    (\forall integer k; 0 <= k < AKMSG_MAXARGS ==> m->am_args[k] == 0) &&
    (\forall integer k; 0 <= k <= AKMSG_STRMAX ==> m->am_str[k] == 0);
*/

/*
 * Copy the layout of a type into *ml.  A copy rather than a pointer
 * into the table: nothing written while a message is encoded or
 * decoded can then change the bounds it is checked against.
 */
/*@
  requires \valid(ml);
  assigns *ml;
  ensures \result == 0 || \result == -1;
  ensures \result == 0 ==> ml->ml_type == type &&
    ml->ml_nargs <= AKMSG_MAXARGS && ml->ml_strmax <= AKMSG_STRMAX;
*/
static int
akmsg_layout(uint32_t type, struct akmsg_layout *ml)
{
	size_t i;

	/*@
	  loop invariant 0 <= i <= AKMSG_NLAYOUTS;
	  loop assigns i;
	  loop variant AKMSG_NLAYOUTS - i;
	*/
	for (i = 0; i < AKMSG_NLAYOUTS; i++) {
		if (akmsg_layouts[i].ml_type == type) {
			*ml = akmsg_layouts[i];
			return 0;
		}
	}
	return -1;
}

/*
 * Is c allowed inside a string field?  NUL is never allowed; other
 * control characters only in free text, and there only as layout.
 */
/*@ assigns \nothing; */
static int
akmsg_okbyte(uint8_t c, uint32_t flags)
{

	if (c == '\0' || c == 0x7f)
		return 0;
	if (c < 0x20)
		return (flags & ML_TEXT) != 0 && (c == '\n' || c == '\t');
	return 1;
}

/*@
  requires \valid(m);
  assigns *m;
  ensures m->am_type == type && m->am_seq == 0 && m->am_len == 0;
  ensures \forall integer k; 0 <= k < AKMSG_MAXARGS ==> m->am_args[k] == 0;
  ensures \forall integer k; 0 <= k <= AKMSG_STRMAX ==> m->am_str[k] == 0;
*/
void
akmsg_init(struct akmsg *m, uint32_t type)
{

	akmsg_clear(m);
	m->am_type = type;
}

/*
 * Set the string of a message.  Fails if it does not fit; the caller
 * decides whether truncation is acceptable.
 */
int
akmsg_setstr(struct akmsg *m, const char *s)
{
	size_t len;

	len = strlen(s);
	if (len > AKMSG_STRMAX)
		return -1;
	(void)memcpy(m->am_str, s, len);
	m->am_str[len] = '\0';
	m->am_len = (uint32_t)len;
	return 0;
}

/*
 * Serialise m into buf, which must hold AKMSG_SIZE bytes.
 */
/*@
  requires \valid_read(m);
  requires \valid(buf + (0 .. AKMSG_SIZE - 1));
  requires \separated(m, buf + (0 .. AKMSG_SIZE - 1));
  assigns buf[0 .. AKMSG_SIZE - 1];
  ensures \result == 0 || \result == -1;
*/
int
akmsg_encode(const struct akmsg *m, uint8_t *buf)
{
	struct akmsg_layout ml;
	size_t off, j;
	uint32_t i;

	/*
	 * Byte loops instead of memset(3) and memcpy(3) here and below:
	 * they keep the typed memory model of WP applicable.
	 */
	/*@
	  loop invariant 0 <= j <= AKMSG_SIZE;
	  loop assigns j, buf[0 .. AKMSG_SIZE - 1];
	  loop variant AKMSG_SIZE - j;
	*/
	for (j = 0; j < AKMSG_SIZE; j++)
		buf[j] = 0;
	if (akmsg_layout(m->am_type, &ml) == -1)
		return -1;
	if (ml.ml_strmax == 0 ? m->am_len != 0 : m->am_len > ml.ml_strmax)
		return -1;

	le32enc(buf, m->am_type);
	le32enc(buf + 4, m->am_seq);
	off = AKMSG_HDRLEN;
	/*@
	  loop invariant 0 <= i <= ml.ml_nargs;
	  loop invariant off == AKMSG_HDRLEN + 4 * i;
	  loop assigns i, off, buf[AKMSG_HDRLEN .. AKMSG_SIZE - 1];
	  loop variant ml.ml_nargs - i;
	*/
	for (i = 0; i < ml.ml_nargs; i++) {
		le32enc(buf + off, m->am_args[i]);
		off += 4;
	}
	if (ml.ml_strmax != 0) {
		le32enc(buf + off, m->am_len);
		off += 4;
		/*@
		  loop invariant 0 <= j <= m->am_len;
		  loop assigns j, buf[off .. off + m->am_len - 1];
		  loop variant m->am_len - j;
		*/
		for (j = 0; j < m->am_len; j++)
			buf[off + j] = (uint8_t)m->am_str[j];
	}
	return 0;
}

/*
 * Parse and validate a message received from the peer.  buf holds
 * exactly AKMSG_SIZE bytes of untrusted data.  On success m contains
 * a NUL terminated string free of forbidden bytes; on failure m is
 * cleared.
 */
/*@
  requires \valid_read(buf + (0 .. AKMSG_SIZE - 1));
  requires \valid(m);
  requires \separated(m, buf + (0 .. AKMSG_SIZE - 1));
  assigns *m;
  ensures AKMSG_OK <= \result <= AKMSG_EBADPAD;
  ensures \result == AKMSG_OK ==>
    m->am_len <= AKMSG_STRMAX && m->am_str[m->am_len] == '\0';
  ensures \result != AKMSG_OK ==> cleared(m);
*/
int
akmsg_decode(const uint8_t *buf, struct akmsg *m)
{
	struct akmsg_layout ml;
	size_t off, end, j;
	uint32_t i, len;
	int error;

	akmsg_init(m, AKMSG_NONE);
	m->am_type = le32dec(buf);
	m->am_seq = le32dec(buf + 4);
	if (akmsg_layout(m->am_type, &ml) == -1) {
		error = AKMSG_EBADTYPE;
		goto bad;
	}

	off = AKMSG_HDRLEN;
	/*@
	  loop invariant 0 <= i <= ml.ml_nargs;
	  loop invariant off == AKMSG_HDRLEN + 4 * i;
	  loop assigns i, off, m->am_args[0 .. AKMSG_MAXARGS - 1];
	  loop variant ml.ml_nargs - i;
	*/
	for (i = 0; i < ml.ml_nargs; i++) {
		m->am_args[i] = le32dec(buf + off);
		off += 4;
	}
	if (ml.ml_nargs > 0 &&
	    (m->am_args[0] < ml.ml_argmin || m->am_args[0] > ml.ml_argmax)) {
		error = AKMSG_EBADARG;
		goto bad;
	}

	end = off;
	if (ml.ml_strmax != 0) {
		len = le32dec(buf + off);
		off += 4;
		if (len > ml.ml_strmax) {
			error = AKMSG_EBADLEN;
			goto bad;
		}
		/*@
		  loop invariant 0 <= j <= len;
		  loop assigns j, m->am_str[0 .. AKMSG_STRMAX];
		  loop variant len - j;
		*/
		for (j = 0; j < len; j++) {
			if (!akmsg_okbyte(buf[off + j], ml.ml_flags)) {
				error = AKMSG_EBADSTR;
				goto bad;
			}
			m->am_str[j] = (char)buf[off + j];
		}
		m->am_str[len] = '\0';
		m->am_len = len;
		end = off + len;
	}

	/*@
	  loop invariant end <= j <= AKMSG_SIZE;
	  loop assigns j;
	  loop variant AKMSG_SIZE - j;
	*/
	for (j = end; j < AKMSG_SIZE; j++) {
		if (buf[j] != 0) {
			error = AKMSG_EBADPAD;
			goto bad;
		}
	}
	return AKMSG_OK;

bad:
	akmsg_clear(m);
	return error;
}

/*
 * Erase a message that may have carried a secret.  The stores go
 * through a volatile pointer, so that the compiler cannot drop them
 * even when the message is not used any more.
 */
/*@
  requires \valid(m);
  assigns *m;
  ensures cleared(m);
*/
void
akmsg_clear(struct akmsg *m)
{
	volatile struct akmsg *vm;
	size_t i;

	vm = m;
	vm->am_type = AKMSG_NONE;
	vm->am_seq = 0;
	/*@
	  loop invariant 0 <= i <= AKMSG_MAXARGS;
	  loop invariant \forall integer k; 0 <= k < i ==> m->am_args[k] == 0;
	  loop assigns i, m->am_args[0 .. AKMSG_MAXARGS - 1];
	  loop variant AKMSG_MAXARGS - i;
	*/
	for (i = 0; i < AKMSG_MAXARGS; i++)
		vm->am_args[i] = 0;
	vm->am_len = 0;
	/*@
	  loop invariant 0 <= i <= AKMSG_STRMAX + 1;
	  loop invariant \forall integer k; 0 <= k < i ==> m->am_str[k] == 0;
	  loop assigns i, m->am_str[0 .. AKMSG_STRMAX];
	  loop variant AKMSG_STRMAX + 1 - i;
	*/
	for (i = 0; i <= AKMSG_STRMAX; i++)
		vm->am_str[i] = '\0';
}

const char *
akmsg_strerror(int error)
{

	switch (error) {
	case AKMSG_OK:
		return "no error";
	case AKMSG_EBADTYPE:
		return "unknown message type";
	case AKMSG_EBADARG:
		return "argument out of range";
	case AKMSG_EBADLEN:
		return "inconsistent string length";
	case AKMSG_EBADSTR:
		return "forbidden byte in string";
	case AKMSG_EBADPAD:
		return "trailing garbage";
	default:
		return "unknown error";
	}
}
