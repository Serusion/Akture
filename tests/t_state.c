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

#include <atf-c.h>

#include "akture.h"

static const int events[] = {
	AKE_BEGIN_AUTH, AKE_RESPONSE, AKE_CANCEL, AKE_POWER,
	AKE_AUTH_OK, AKE_AUTH_FAIL, AKE_SESSION_END,
};

ATF_TC(session_needs_auth);
ATF_TC_HEAD(session_needs_auth, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "AKS_SESSION is only reached from AKS_AUTH on AKE_AUTH_OK");
}
ATF_TC_BODY(session_needs_auth, tc)
{
	int s, next;
	size_t i;

	for (s = AKS_IDLE; s <= AKS_SESSION; s++) {
		for (i = 0; i < __arraycount(events); i++) {
			next = -1;
			if (akstate_next(s, events[i], &next) == -1) {
				ATF_CHECK_EQ(next, -1);
				continue;
			}
			ATF_CHECK(akstate_valid(next));
			if (next == AKS_SESSION)
				ATF_CHECK(s == AKS_AUTH &&
				    events[i] == AKE_AUTH_OK);
		}
	}
}

ATF_TC(accepts);
ATF_TC_HEAD(accepts, tc)
{
	atf_tc_set_md_var(tc, "descr", "messages accepted per state");
}
ATF_TC_BODY(accepts, tc)
{

	ATF_CHECK(akstate_accepts(AKS_IDLE, AKMSG_BEGIN_AUTH));
	ATF_CHECK(akstate_accepts(AKS_IDLE, AKMSG_POWER));
	ATF_CHECK(!akstate_accepts(AKS_IDLE, AKMSG_RESPONSE));
	ATF_CHECK(akstate_accepts(AKS_AUTH, AKMSG_RESPONSE));
	ATF_CHECK(!akstate_accepts(AKS_AUTH, AKMSG_BEGIN_AUTH));
	ATF_CHECK(!akstate_accepts(AKS_AUTH, AKMSG_POWER));
	ATF_CHECK(!akstate_accepts(AKS_SESSION, AKMSG_CANCEL));
	ATF_CHECK(!akstate_accepts(AKS_IDLE, AKMSG_INIT));
}

ATF_TC(delay);
ATF_TC_HEAD(delay, tc)
{
	atf_tc_set_md_var(tc, "descr", "failure delay grows and saturates");
}
ATF_TC_BODY(delay, tc)
{

	ATF_CHECK_EQ(akstate_fail_delay(1), 0);
	ATF_CHECK_EQ(akstate_fail_delay(2), 0);
	ATF_CHECK_EQ(akstate_fail_delay(3), 2);
	ATF_CHECK_EQ(akstate_fail_delay(4), 4);
	ATF_CHECK_EQ(akstate_fail_delay(16), 28);
	ATF_CHECK_EQ(akstate_fail_delay(17), 30);
	ATF_CHECK_EQ(akstate_fail_delay(1000), 30);
	ATF_CHECK_EQ(akstate_fail_delay(UINT32_MAX), 30);
}

ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, session_needs_auth);
	ATF_TP_ADD_TC(tp, accepts);
	ATF_TP_ADD_TC(tp, delay);
	return atf_no_error();
}
