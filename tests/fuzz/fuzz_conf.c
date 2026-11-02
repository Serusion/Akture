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

/* libFuzzer harness for the configuration reader of the master. */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "akparse.h"
#include "akture.h"

int	LLVMFuzzerTestOneInput(const uint8_t *, size_t);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	static struct akconf ac;
	char line[AKPARSE_LINEMAX];
	const char *why;
	size_t off, len;

	conf_defaults(&ac);
	for (off = 0; off < size; off += len + 1) {
		for (len = 0; off + len < size && data[off + len] != '\n' &&
		    len < sizeof(line) - 1; len++)
			continue;
		memcpy(line, data + off, len);
		line[len] = '\0';
		(void)conf_line(&ac, line, &why);
	}
	return 0;
}
