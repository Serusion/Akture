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
#include <sys/socket.h>

#include <errno.h>
#include <string.h>
#include <syslog.h>

#include "akparse.h"
#include "akutil.h"
#include "greeter.h"

/*
 * Greeter side of the socketpair to the master.
 */

int
ipc_send(struct greeter *g, struct akmsg *m)
{
	uint8_t buf[AKMSG_SIZE];
	ssize_t n;
	int error;

	m->am_seq = g->g_sseq++;
	error = akmsg_encode(m, buf);
	akmsg_clear(m);
	if (error == -1)
		return -1;
	n = send(g->g_sock, buf, sizeof(buf), 0);
	ak_explicit_zero(buf, sizeof(buf));
	if (n != (ssize_t)sizeof(buf)) {
		syslog(LOG_ERR, "send: %s", n == -1 ? strerror(errno) :
		    "short write");
		return -1;
	}
	return 0;
}

int
ipc_recv(struct greeter *g, struct akmsg *m)
{
	uint8_t buf[AKMSG_SIZE + 1];
	ssize_t n;
	int error;

	if ((n = recv(g->g_sock, buf, sizeof(buf), 0)) != AKMSG_SIZE) {
		if (n != 0)
			syslog(LOG_ERR, "recv: %s", n == -1 ?
			    strerror(errno) : "bad message size");
		return -1;
	}
	error = akmsg_decode(buf, m);
	ak_explicit_zero(buf, sizeof(buf));
	if (error != AKMSG_OK) {
		syslog(LOG_ERR, "bad message: %s", akmsg_strerror(error));
		return -1;
	}
	if (m->am_seq != g->g_rseq++) {
		syslog(LOG_ERR, "message out of sequence");
		return -1;
	}
	return 0;
}

/*
 * Receive the options, the sessions and finally AKMSG_INIT.  Values
 * are checked again: the master is trusted, but the greeter must not
 * crash on anything.
 */
int
ipc_init(struct greeter *g)
{
	struct akmsg m;
	unsigned int pct;
	int i, b;

	for (i = 0; i < AKOPT_NKEYS; i++)
		(void)strlcpy(g->g_opt[i], akopt_table[i].ao_default,
		    sizeof(g->g_opt[i]));

	for (;;) {
		if (ipc_recv(g, &m) == -1)
			return -1;
		switch (m.am_type) {
		case AKMSG_INIT_OPTION:
			if (m.am_args[0] < AKOPT_NKEYS &&
			    akopt_valid((int)m.am_args[0], m.am_str))
				(void)strlcpy(g->g_opt[m.am_args[0]], m.am_str,
				    sizeof(g->g_opt[0]));
			break;
		case AKMSG_INIT_SESSION:
			(void)strlcpy(g->g_sessions[m.am_args[0]], m.am_str,
			    sizeof(g->g_sessions[0]));
			break;
		case AKMSG_INIT:
			goto done;
		default:
			syslog(LOG_ERR, "unexpected message %u", m.am_type);
			return -1;
		}
	}

done:
	g->g_flags = m.am_args[0];
	if (m.am_args[1] < 1 || m.am_args[1] > AK_MAXSESSIONS ||
	    m.am_args[2] >= m.am_args[1]) {
		syslog(LOG_ERR, "bad session list");
		return -1;
	}
	g->g_nsessions = (int)m.am_args[1];
	g->g_session = (int)m.am_args[2];
	(void)strlcpy(g->g_lastuser, m.am_str, sizeof(g->g_lastuser));

	(void)ak_parse_color(g->g_opt[AKOPT_BACKGROUND_COLOR], &g->g_bgcolor);
	(void)ak_parse_color(g->g_opt[AKOPT_ACCENT], &g->g_accent);
	(void)ak_parse_color(g->g_opt[AKOPT_TEXT_COLOR], &g->g_text);
	pct = 100;
	(void)ak_parse_scale(g->g_opt[AKOPT_SCALE], &pct);
	g->g_scale = pct / 100.0;
	b = 1;
	(void)ak_parse_bool(g->g_opt[AKOPT_SHOW_LOGO], &b);
	g->g_showlogo = b;
	return 0;
}
