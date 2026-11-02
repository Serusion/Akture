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

/*
 * Entry points for Frama-C/Eva: feed the parsers of untrusted input
 * with arbitrary bytes and let Eva show that no run-time error can
 * occur.  Not part of any program.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "__fc_builtin.h"

#include "akbg.h"
#include "akmsg.h"
#include "akture.h"

#define EVA_LINEMAX	64

static void
eva_akbg(void)
{
	uint8_t hdr[AKBG_HDRLEN];
	struct akbg_header h;

	Frama_C_make_unknown((char *)hdr, sizeof(hdr));
	(void)akbg_check(hdr, Frama_C_unsigned_long_long_interval(0,
	    UINT64_MAX), Frama_C_unsigned_int_interval(0, UINT32_MAX),
	    Frama_C_unsigned_int_interval(0, UINT32_MAX), &h);
}

static void
eva_akmsg(void)
{
	uint8_t buf[AKMSG_SIZE];
	struct akmsg m;

	Frama_C_make_unknown((char *)buf, sizeof(buf));
	(void)akmsg_decode(buf, &m);
}

/*
 * The whole configuration reader, from the file: the Frama-C libc
 * models fopen(3) and fgets(3), so every line is arbitrary.
 */
static void
eva_conf(void)
{
	static struct akconf ac;

	(void)conf_load(&ac, "/etc/akture/akture.conf");
}

/*
 * The splitting of x_args and session commands into argv.
 */
static void
eva_split(void)
{
	char buf[EVA_LINEMAX], *argv[AK_MAXARGS];

	Frama_C_make_unknown(buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	(void)ak_split_words(buf, argv, AK_MAXARGS);
}

int
main(void)
{

	eva_akbg();
	eva_akmsg();
	eva_conf();
	eva_split();
	return 0;
}
