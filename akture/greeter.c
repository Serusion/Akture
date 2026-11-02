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
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * Master side of the greeter: start it as the unprivileged user,
 * talk to it over a SOCK_SEQPACKET socketpair and stop it again.
 */

#define GREETER_SNDTIMEO	5	/* seconds a send may block */

static int	greeter_init(struct akture *);

static __dead void
greeter_exec(int fd, const struct akcred *cred)
{
	static char *const argv[] = { __UNCONST("akture-greeter"), NULL };
	static char *const envp[] = {
		__UNCONST("PATH=/usr/bin:/bin"),
		__UNCONST("DISPLAY=" AKTURE_DISPLAY),
		__UNCONST("XAUTHORITY=" _PATH_AKTURE_GREETERAUTH),
		__UNCONST("HOME=" _PATH_AKTURE_DB),
		NULL
	};

	if (setsid() == -1)
		_exit(AKTURE_EXIT_ERROR);
	if (fd != AKTURE_GREETER_FD) {
		if (dup2(fd, AKTURE_GREETER_FD) == -1)
			_exit(AKTURE_EXIT_ERROR);
	}
	closefrom(AKTURE_GREETER_FD + 1);
	if (ak_privdrop(cred) == -1) {
		syslog(LOG_ERR, "cannot drop privileges for the greeter");
		_exit(AKTURE_EXIT_ERROR);
	}
	(void)execve(_PATH_AKTURE_GREETER, argv, envp);
	syslog(LOG_ERR, "%s: %m", _PATH_AKTURE_GREETER);
	_exit(AKTURE_EXIT_ERROR);
}

int
greeter_start(struct akture *a)
{
	struct timeval tv;
	int sv[2];
	pid_t pid;

	if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, sv) == -1) {
		syslog(LOG_ERR, "socketpair: %m");
		return -1;
	}
	if ((pid = child_spawn(a, CHILD_GREETER)) == -1) {
		(void)close(sv[0]);
		(void)close(sv[1]);
		return -1;
	}
	if (pid == 0) {
		(void)close(sv[0]);
		greeter_exec(sv[1], &a->a_gcred);
	}
	(void)close(sv[1]);

	a->a_gsock = sv[0];
	a->a_gsseq = 0;
	a->a_grseq = 0;
	(void)fcntl(a->a_gsock, F_SETFD, FD_CLOEXEC);
	tv.tv_sec = GREETER_SNDTIMEO;
	tv.tv_usec = 0;
	(void)setsockopt(a->a_gsock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (ev_watch(a, a->a_gsock, 1) == -1 || greeter_init(a) == -1) {
		greeter_stop(a);
		return -1;
	}
	return 0;
}

/*
 * Stop the greeter: disconnect first so that it cannot send anything
 * more, then terminate it.
 */
void
greeter_stop(struct akture *a)
{

	if (a->a_gsock != -1) {
		(void)ev_watch(a, a->a_gsock, 0);
		(void)close(a->a_gsock);
		a->a_gsock = -1;
	}
	child_stop(a, CHILD_GREETER);
}

/*
 * Send a message and erase both copies of it.
 */
int
greeter_send(struct akture *a, struct akmsg *m)
{
	uint8_t buf[AKMSG_SIZE];
	ssize_t n;
	int error;

	m->am_seq = a->a_gsseq++;
	error = akmsg_encode(m, buf);
	akmsg_clear(m);
	if (error == -1) {
		syslog(LOG_ERR, "cannot encode a message for the greeter");
		return -1;
	}
	n = send(a->a_gsock, buf, sizeof(buf), 0);
	ak_explicit_zero(buf, sizeof(buf));
	if (n != (ssize_t)sizeof(buf)) {
		syslog(LOG_ERR, "send to greeter: %s",
		    n == -1 ? strerror(errno) : "short write");
		return -1;
	}
	return 0;
}

/*
 * Receive one message.  Returns -1 if the greeter is gone or sent
 * anything malformed or out of sequence; the caller kills it then.
 * Whether the type is acceptable in the current state is checked by
 * the caller with akstate_accepts().
 */
int
greeter_recv(struct akture *a, struct akmsg *m)
{
	uint8_t buf[AKMSG_SIZE + 1];
	ssize_t n;
	int error;

	n = recv(a->a_gsock, buf, sizeof(buf), 0);
	if (n == 0) {
		akmsg_clear(m);
		return -1;
	}
	if (n != AKMSG_SIZE) {
		ak_explicit_zero(buf, sizeof(buf));
		akmsg_clear(m);
		syslog(LOG_WARNING, "greeter sent a message of %zd bytes", n);
		return -1;
	}
	error = akmsg_decode(buf, m);
	ak_explicit_zero(buf, sizeof(buf));
	if (error != AKMSG_OK) {
		syslog(LOG_WARNING, "bad message from greeter: %s",
		    akmsg_strerror(error));
		return -1;
	}
	if (m->am_seq != a->a_grseq) {
		akmsg_clear(m);
		syslog(LOG_WARNING, "greeter message out of sequence");
		return -1;
	}
	a->a_grseq++;
	return 0;
}

int
greeter_result(struct akture *a, uint32_t result)
{
	struct akmsg m;

	akmsg_init(&m, AKMSG_AUTH_RESULT);
	m.am_args[0] = result;
	return greeter_send(a, &m);
}

/*
 * Tell the greeter everything it must not read by itself: the
 * options meant for it, the sessions and the last user.
 */
static int
greeter_init(struct akture *a)
{
	char last[AKMSG_NAMEMAX + 1];
	struct akmsg m;
	int i, def;

	for (i = 0; i < AKOPT_NKEYS; i++) {
		if ((akopt_table[i].ao_flags & AKOPT_F_GREETER) == 0)
			continue;
		akmsg_init(&m, AKMSG_INIT_OPTION);
		m.am_args[0] = (uint32_t)i;
		(void)akmsg_setstr(&m, conf_str(a, i));
		if (greeter_send(a, &m) == -1)
			return -1;
	}

	for (i = 0; i < a->a_nsessions; i++) {
		akmsg_init(&m, AKMSG_INIT_SESSION);
		m.am_args[0] = (uint32_t)i;
		(void)akmsg_setstr(&m, a->a_sessions[i].s_name);
		if (greeter_send(a, &m) == -1)
			return -1;
	}

	last[0] = '\0';
	if (conf_bool(a, AKOPT_REMEMBER_USER))
		(void)lastuser_read(last, sizeof(last));
	if ((def = session_lookup(a, conf_str(a, AKOPT_DEFAULT_SESSION))) == -1)
		def = 0;

	akmsg_init(&m, AKMSG_INIT);
	m.am_args[0] = a->a_initflags;
	if (conf_bool(a, AKOPT_ALLOW_POWER))
		m.am_args[0] |= AKINIT_POWER;
	m.am_args[1] = (uint32_t)a->a_nsessions;
	m.am_args[2] = (uint32_t)def;
	(void)akmsg_setstr(&m, last);
	a->a_initflags = 0;
	return greeter_send(a, &m);
}
