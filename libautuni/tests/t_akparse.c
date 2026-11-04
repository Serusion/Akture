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

#include "akparse.h"

ATF_TC(parse_line);
ATF_TC_HEAD(parse_line, tc)
{
	atf_tc_set_md_var(tc, "descr", "key = value lines and comments");
}
ATF_TC_BODY(parse_line, tc)
{
	char k[AKPARSE_LINEMAX], v[AKPARSE_LINEMAX];

	ATF_CHECK_EQ(ak_parse_line("  # comment = no\n", k, v),
	    AKPARSE_EMPTY);
	ATF_CHECK_EQ(ak_parse_line("\n", k, v), AKPARSE_EMPTY);
	ATF_CHECK_EQ(ak_parse_line("", k, v), AKPARSE_EMPTY);

	ATF_REQUIRE_EQ(ak_parse_line("accent = #C8441B # not a comment\n",
	    k, v), AKPARSE_PAIR);
	ATF_CHECK_STREQ(k, "accent");
	ATF_CHECK_STREQ(v, "#C8441B # not a comment");

	ATF_REQUIRE_EQ(ak_parse_line("\tkey=  \t\n", k, v), AKPARSE_PAIR);
	ATF_CHECK_STREQ(k, "key");
	ATF_CHECK_STREQ(v, "");

	ATF_CHECK_EQ(ak_parse_line("Bad = 1", k, v), AKPARSE_ERROR);
	ATF_CHECK_EQ(ak_parse_line("novalue", k, v), AKPARSE_ERROR);
	ATF_CHECK_EQ(ak_parse_line("key value", k, v), AKPARSE_ERROR);
}

ATF_TC(values);
ATF_TC_HEAD(values, tc)
{
	atf_tc_set_md_var(tc, "descr", "colours, booleans and scales");
}
ATF_TC_BODY(values, tc)
{
	uint32_t c;
	unsigned int pct;
	int b;

	ATF_REQUIRE_EQ(ak_parse_color("#1E1410", &c), 0);
	ATF_CHECK_EQ(c, 0xff1e1410U);
	ATF_CHECK_EQ(ak_parse_color("1E1410", &c), -1);
	ATF_CHECK_EQ(ak_parse_color("#1E141", &c), -1);
	ATF_CHECK_EQ(ak_parse_color("#1E14100", &c), -1);
	ATF_CHECK_EQ(ak_parse_color("#GG0000", &c), -1);

	ATF_REQUIRE_EQ(ak_parse_bool("yes", &b), 0);
	ATF_CHECK_EQ(b, 1);
	ATF_REQUIRE_EQ(ak_parse_bool("no", &b), 0);
	ATF_CHECK_EQ(b, 0);
	ATF_CHECK_EQ(ak_parse_bool("maybe", &b), -1);

	ATF_REQUIRE_EQ(ak_parse_scale("1.25", &pct), 0);
	ATF_CHECK_EQ(pct, 125);
	ATF_CHECK_EQ(ak_parse_scale("3", &pct), -1);
}

ATF_TC(split);
ATF_TC_HEAD(split, tc)
{
	atf_tc_set_md_var(tc, "descr", "commands are split at blanks");
}
ATF_TC_BODY(split, tc)
{
	char buf[64], *argv[4];

	(void)strlcpy(buf, "  /usr/bin/X  -nolisten tcp ", sizeof(buf));
	ATF_REQUIRE_EQ(ak_split_words(buf, argv, 4), 3);
	ATF_CHECK_STREQ(argv[0], "/usr/bin/X");
	ATF_CHECK_STREQ(argv[2], "tcp");
	ATF_CHECK(argv[3] == NULL);

	(void)strlcpy(buf, "a b c d", sizeof(buf));
	ATF_CHECK_EQ(ak_split_words(buf, argv, 4), -1);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, parse_line);
	ATF_TP_ADD_TC(tp, values);
	ATF_TP_ADD_TC(tp, split);
	return atf_no_error();
}
