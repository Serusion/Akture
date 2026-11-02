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

#include "akture.h"

/*
 * The state machine of the master.  It is kept free of side effects so
 * that Frama-C/WP can prove, independently of PAM and the kernel, that
 *
 *	- only the messages listed for a state are accepted in it, and
 *	- AKS_SESSION is entered only from AKS_AUTH on AKE_AUTH_OK.
 *
 * main.c and auth.c obtain every new state from akstate_next(), and
 * session_run() requires AKS_SESSION.
 */

/*@
  predicate valid_state(integer s) =
    s == AKS_IDLE || s == AKS_AUTH || s == AKS_SESSION;
*/

/*@
  assigns \nothing;
  ensures \result <==> valid_state(s);
*/
int
akstate_valid(int s)
{

	return s == AKS_IDLE || s == AKS_AUTH || s == AKS_SESSION;
}

/*
 * May a message of the given type be received in state s?  Anything
 * else is a protocol violation and gets the greeter killed.
 */
/*@
  assigns \nothing;
  ensures \result == 0 || \result == 1;
  ensures \result == 1 && s == AKS_IDLE ==>
    type == AKMSG_BEGIN_AUTH || type == AKMSG_POWER ||
    type == AKMSG_CANCEL;
  ensures \result == 1 && s == AKS_AUTH ==>
    type == AKMSG_RESPONSE || type == AKMSG_CANCEL;
  ensures s == AKS_SESSION ==> \result == 0;
*/
int
akstate_accepts(int s, uint32_t type)
{

	switch (s) {
	case AKS_IDLE:
		/* A CANCEL may cross the result of the attempt it cancels. */
		return type == AKMSG_BEGIN_AUTH || type == AKMSG_POWER ||
		    type == AKMSG_CANCEL;
	case AKS_AUTH:
		return type == AKMSG_RESPONSE || type == AKMSG_CANCEL;
	default:
		return 0;
	}
}

/*
 * Compute the state that follows s on event ev.  Returns -1, leaving
 * *next alone, if ev is not allowed in s.
 */
/*@
  requires valid_state(s);
  requires \valid(next);
  assigns *next;
  ensures \result == 0 || \result == -1;
  ensures \result == 0 ==> valid_state(*next);
  ensures \result == 0 && *next == AKS_SESSION ==>
    s == AKS_AUTH && ev == AKE_AUTH_OK;
  ensures \result == -1 ==> *next == \old(*next);
*/
int
akstate_next(int s, int ev, int *next)
{

	switch (s) {
	case AKS_IDLE:
		switch (ev) {
		case AKE_BEGIN_AUTH:
			*next = AKS_AUTH;
			return 0;
		case AKE_POWER:
		case AKE_CANCEL:
			*next = AKS_IDLE;
			return 0;
		}
		break;
	case AKS_AUTH:
		switch (ev) {
		case AKE_RESPONSE:
		case AKE_CANCEL:
			*next = AKS_AUTH;
			return 0;
		case AKE_AUTH_OK:
			*next = AKS_SESSION;
			return 0;
		case AKE_AUTH_FAIL:
			*next = AKS_IDLE;
			return 0;
		}
		break;
	case AKS_SESSION:
		if (ev == AKE_SESSION_END) {
			*next = AKS_IDLE;
			return 0;
		}
		break;
	}
	return -1;
}

/*
 * Seconds to wait before reporting the n-th consecutive failure.
 */
/*@
  assigns \nothing;
  ensures 0 <= \result <= AK_DELAY_MAX;
  ensures n < AK_DELAY_FIRST ==> \result == 0;
*/
unsigned int
akstate_fail_delay(unsigned int n)
{
	unsigned int steps;

	if (n < AK_DELAY_FIRST)
		return 0;
	steps = n - AK_DELAY_FIRST + 1;
	if (steps >= AK_DELAY_MAX / AK_DELAY_STEP)
		return AK_DELAY_MAX;
	return steps * AK_DELAY_STEP;
}
