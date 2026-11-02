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

#include <atf-c.h>

#include "akmsg.h"

static void
encode(uint32_t type, uint32_t arg, const char *s, uint8_t *buf)
{
	struct akmsg m;

	akmsg_init(&m, type);
	m.am_seq = 7;
	m.am_args[0] = arg;
	if (s != NULL)
		ATF_REQUIRE_EQ(akmsg_setstr(&m, s), 0);
	ATF_REQUIRE_EQ(akmsg_encode(&m, buf), 0);
}

ATF_TC(roundtrip);
ATF_TC_HEAD(roundtrip, tc)
{
	atf_tc_set_md_var(tc, "descr", "encode and decode agree");
}
ATF_TC_BODY(roundtrip, tc)
{
	uint8_t buf[AKMSG_SIZE];
	struct akmsg m;

	encode(AKMSG_BEGIN_AUTH, 3, "user", buf);
	ATF_REQUIRE_EQ(akmsg_decode(buf, &m), AKMSG_OK);
	ATF_CHECK_EQ(m.am_type, AKMSG_BEGIN_AUTH);
	ATF_CHECK_EQ(m.am_seq, 7);
	ATF_CHECK_EQ(m.am_args[0], 3);
	ATF_CHECK_EQ(m.am_len, 4);
	ATF_CHECK_STREQ(m.am_str, "user");

	encode(AKMSG_PROMPT, AKPROMPT_INFO, "line 1\nline 2", buf);
	ATF_REQUIRE_EQ(akmsg_decode(buf, &m), AKMSG_OK);
	ATF_CHECK_STREQ(m.am_str, "line 1\nline 2");
}

ATF_TC(reject);
ATF_TC_HEAD(reject, tc)
{
	atf_tc_set_md_var(tc, "descr", "malformed messages are rejected");
}
ATF_TC_BODY(reject, tc)
{
	uint8_t buf[AKMSG_SIZE];
	struct akmsg m;

	/* Unknown type */
	encode(AKMSG_CANCEL, 0, NULL, buf);
	buf[0] = 99;
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADTYPE);

	/* Argument out of range */
	encode(AKMSG_POWER, AKPOWER_REBOOT, NULL, buf);
	buf[8] = 9;
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADARG);
	encode(AKMSG_BEGIN_AUTH, 0, "u", buf);
	buf[8] = AK_MAXSESSIONS;
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADARG);

	/* Length beyond the field */
	encode(AKMSG_RESPONSE, 0, "secret", buf);
	buf[8] = 0xff;
	buf[9] = 0xff;
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADLEN);

	/* NUL and control characters inside a string */
	encode(AKMSG_BEGIN_AUTH, 0, "user", buf);
	buf[16 + 1] = '\0';
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADSTR);
	encode(AKMSG_BEGIN_AUTH, 0, "user", buf);
	buf[16 + 1] = '\n';
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADSTR);

	/* Bytes after the used part */
	encode(AKMSG_CANCEL, 0, NULL, buf);
	buf[AKMSG_SIZE - 1] = 1;
	ATF_CHECK_EQ(akmsg_decode(buf, &m), AKMSG_EBADPAD);
}

ATF_TC(cleared);
ATF_TC_HEAD(cleared, tc)
{
	atf_tc_set_md_var(tc, "descr", "a rejected message leaves no data");
}
ATF_TC_BODY(cleared, tc)
{
	uint8_t buf[AKMSG_SIZE];
	struct akmsg m;
	size_t i;

	encode(AKMSG_RESPONSE, 0, "secret", buf);
	buf[AKMSG_SIZE - 1] = 1;
	ATF_REQUIRE(akmsg_decode(buf, &m) != AKMSG_OK);
	for (i = 0; i < sizeof(m.am_str); i++)
		ATF_REQUIRE_EQ(m.am_str[i], 0);
	ATF_CHECK_EQ(m.am_len, 0);
}

ATF_TC(encode_limits);
ATF_TC_HEAD(encode_limits, tc)
{
	atf_tc_set_md_var(tc, "descr", "strings longer than the field fail");
}
ATF_TC_BODY(encode_limits, tc)
{
	char name[AKMSG_NAMEMAX + 2];
	uint8_t buf[AKMSG_SIZE];
	struct akmsg m;

	memset(name, 'a', sizeof(name) - 1);
	name[sizeof(name) - 1] = '\0';
	akmsg_init(&m, AKMSG_BEGIN_AUTH);
	ATF_REQUIRE_EQ(akmsg_setstr(&m, name), 0);
	ATF_CHECK_EQ(akmsg_encode(&m, buf), -1);
	akmsg_init(&m, AKMSG_CANCEL);
	ATF_REQUIRE_EQ(akmsg_setstr(&m, "x"), 0);
	ATF_CHECK_EQ(akmsg_encode(&m, buf), -1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, roundtrip);
	ATF_TP_ADD_TC(tp, reject);
	ATF_TP_ADD_TC(tp, cleared);
	ATF_TP_ADD_TC(tp, encode_limits);
	return atf_no_error();
}
