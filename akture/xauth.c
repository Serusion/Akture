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

#include <sys/param.h>
#include <sys/endian.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * X authority files.  A record is a big-endian 16-bit family followed
 * by four counted strings (16-bit length, then bytes): address,
 * display number, authorisation name and data.
 */

#define XAUTH_NAME	"MIT-MAGIC-COOKIE-1"

/*
 * Append a counted string.  Returns the new offset, or 0 if it does
 * not fit.
 */
/*@
  requires \valid(buf + (0 .. size - 1));
  requires \valid_read(s + (0 .. len - 1));
  assigns buf[off .. size - 1];
*/
static size_t
xauth_put(uint8_t *buf, size_t size, size_t off, const void *s, size_t len)
{

	if (off == 0 || off > size || len > UINT16_MAX ||
	    size - off < len + 2)
		return 0;
	be16enc(buf + off, (uint16_t)len);
	(void)memcpy(buf + off + 2, s, len);
	return off + 2 + len;
}

static size_t
xauth_record(uint8_t *buf, size_t size, uint16_t family, const char *addr,
    const char *num, const uint8_t *cookie)
{
	size_t off;

	if (size < 2)
		return 0;
	be16enc(buf, family);
	off = xauth_put(buf, size, 2, addr, strlen(addr));
	off = xauth_put(buf, size, off, num, strlen(num));
	off = xauth_put(buf, size, off, XAUTH_NAME, strlen(XAUTH_NAME));
	return xauth_put(buf, size, off, cookie, AK_COOKIELEN);
}

/*
 * Encode the records for our display: one for the local host by name
 * and a wildcard one, which keeps working if the host is renamed.
 * Returns the length, or 0 if buf is too small.
 */
size_t
xauth_entries(uint8_t *buf, size_t size, const char *host, const char *num,
    const uint8_t *cookie)
{
	size_t n, m;

	if ((n = xauth_record(buf, size, XAUTH_FAMILY_LOCAL, host, num,
	    cookie)) == 0)
		return 0;
	if ((m = xauth_record(buf + n, size - n, XAUTH_FAMILY_WILD, "", num,
	    cookie)) == 0)
		return 0;
	return n + m;
}

/*
 * Copy the records of an existing file except those for display num.
 * A truncated trailing record is dropped.  Returns the output length.
 */
size_t
xauth_filter(const uint8_t *in, size_t inlen, const char *num, uint8_t *out,
    size_t outsize)
{
	size_t off, start, len, numlen, out_off;
	const uint8_t *numfield;
	int i;

	numlen = strlen(num);
	off = out_off = 0;
	while (inlen - off >= 2) {
		start = off;
		off += 2;			/* family */
		numfield = NULL;
		len = 0;
		for (i = 0; i < 4; i++) {
			if (inlen - off < 2)
				return out_off;
			len = be16dec(in + off);
			off += 2;
			if (inlen - off < len)
				return out_off;
			if (i == 1)
				numfield = in + off;
			if (i == 1 && len != numlen)
				numfield = NULL;
			off += len;
		}
		if (numfield != NULL && memcmp(numfield, num, numlen) == 0)
			continue;
		if (outsize - out_off < off - start)
			return out_off;
		(void)memcpy(out + out_off, in + start, off - start);
		out_off += off - start;
	}
	return out_off;
}

/*
 * Write a fresh authority file for our display.
 */
int
xauth_write(const char *path, const uint8_t *cookie, uid_t uid, gid_t gid)
{
	char host[MAXHOSTNAMELEN];
	uint8_t buf[1024];
	size_t len;

	if (gethostname(host, sizeof(host)) == -1)
		return -1;
	host[sizeof(host) - 1] = '\0';
	if ((len = xauth_entries(buf, sizeof(buf), host, AKTURE_DISPLAYNUM,
	    cookie)) == 0)
		return -1;
	return ak_write_file(path, buf, len, 0600, uid, gid);
}

/*
 * Replace the records for our display in the user's authority file,
 * keeping the others.  Runs as the user.
 */
int
xauth_merge(const char *path, const uint8_t *cookie)
{
	static uint8_t in[XAUTH_BUFSIZE], out[XAUTH_BUFSIZE + 1024];
	char host[MAXHOSTNAMELEN];
	size_t inlen, outlen, n;
	ssize_t r;
	int fd;

	inlen = 0;
	if ((fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)) != -1) {
		while (inlen < sizeof(in) &&
		    (r = read(fd, in + inlen, sizeof(in) - inlen)) > 0)
			inlen += (size_t)r;
		(void)close(fd);
	} else if (errno != ENOENT)
		return -1;

	if (gethostname(host, sizeof(host)) == -1)
		return -1;
	host[sizeof(host) - 1] = '\0';
	outlen = xauth_filter(in, inlen, AKTURE_DISPLAYNUM, out, XAUTH_BUFSIZE);
	if ((n = xauth_entries(out + outlen, sizeof(out) - outlen, host,
	    AKTURE_DISPLAYNUM, cookie)) == 0)
		return -1;
	return ak_write_file(path, out, outlen + n, 0600, getuid(), getgid());
}
