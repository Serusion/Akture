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
#include <sys/event.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "akture.h"

/*
 * Event handling of the master.  Signals are blocked and collected
 * through kqueue(2) instead of handlers, so all the work happens in
 * the normal flow of control and nothing runs asynchronously.
 */

static const int ev_signals[] = { SIGCHLD, SIGTERM, SIGINT, SIGHUP, SIGUSR1 };

static void	child_reap(struct akture *);

int
ev_init(struct akture *a)
{
	struct kevent kev[__arraycount(ev_signals)];
	sigset_t set;
	size_t i;

	(void)sigemptyset(&set);
	for (i = 0; i < __arraycount(ev_signals); i++) {
		(void)sigaddset(&set, ev_signals[i]);
		EV_SET(&kev[i], ev_signals[i], EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
		return -1;
	/* Writes to a dead greeter must fail, not kill us. */
	(void)signal(SIGPIPE, SIG_IGN);

	if ((a->a_kq = kqueue()) == -1)
		return -1;
	if (fcntl(a->a_kq, F_SETFD, FD_CLOEXEC) == -1)
		return -1;
	if (kevent(a->a_kq, kev, __arraycount(kev), NULL, 0, NULL) == -1)
		return -1;

	for (i = 0; i < CHILD_NKINDS; i++)
		a->a_child[i].c_pid = -1;
	return 0;
}

/*
 * Start or stop watching fd for readability.
 */
int
ev_watch(struct akture *a, int fd, int on)
{
	struct kevent kev;

	EV_SET(&kev, fd, EVFILT_READ, on ? EV_ADD : EV_DELETE, 0, 0, 0);
	return kevent(a->a_kq, &kev, 1, NULL, 0, NULL);
}

/*
 * Wait up to timeout milliseconds (-1: forever) for something to
 * happen and report what it was.  Exited children are reaped here.
 */
int
ev_wait(struct akture *a, int timeout)
{
	struct kevent kev;
	struct timespec ts, *tsp;
	int n;

	if (timeout < 0)
		tsp = NULL;
	else {
		ts.tv_sec = timeout / 1000;
		ts.tv_nsec = (long)(timeout % 1000) * 1000000;
		tsp = &ts;
	}

	n = kevent(a->a_kq, NULL, 0, &kev, 1, tsp);
	if (n == -1) {
		if (errno != EINTR)
			syslog(LOG_ERR, "kevent: %m");
		return EV_TIMEOUT;
	}
	if (n == 0)
		return EV_TIMEOUT;

	if (kev.filter == EVFILT_READ)
		return EV_READ;

	switch (kev.ident) {
	case SIGCHLD:
		child_reap(a);
		return EV_CHILD;
	case SIGUSR1:
		return EV_XREADY;
	default:
		a->a_quit = 1;
		return EV_QUIT;
	}
}

/*
 * Milliseconds left until deadline, for ev_wait().
 */
static int
ms_left(const struct timespec *deadline)
{
	struct timespec now;
	long long ms;

	(void)clock_gettime(CLOCK_MONOTONIC, &now);
	ms = (long long)(deadline->tv_sec - now.tv_sec) * 1000 +
	    (deadline->tv_nsec - now.tv_nsec) / 1000000;
	return ms < 0 ? 0 : (int)ms;
}

static void
child_reap(struct akture *a)
{
	pid_t pid;
	int i, status;

	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		for (i = 0; i < CHILD_NKINDS; i++) {
			if (a->a_child[i].c_pid == pid) {
				a->a_child[i].c_pid = -1;
				a->a_child[i].c_status = status;
			}
		}
		/* Others are hook runners and shutdown(8): ignore them. */
	}
}

/*
 * fork(2) a child.  kind is CHILD_* to keep track of it, or -1 for a
 * child nobody waits for.  The child gets the default signal state.
 */
pid_t
child_spawn(struct akture *a, int kind)
{
	sigset_t set;
	pid_t pid;

	if ((pid = fork()) == -1) {
		syslog(LOG_ERR, "fork: %m");
		return -1;
	}
	if (pid == 0) {
		(void)signal(SIGPIPE, SIG_DFL);
		(void)sigemptyset(&set);
		(void)sigprocmask(SIG_SETMASK, &set, NULL);
		return 0;
	}
	if (kind >= 0) {
		a->a_child[kind].c_pid = pid;
		a->a_child[kind].c_status = 0;
		a->a_child[kind].c_start = time(NULL);
	}
	return pid;
}

int
child_running(const struct akture *a, int kind)
{

	return a->a_child[kind].c_pid != -1;
}

/*
 * Wait up to timeout seconds (-1: forever) for a child to exit.
 * Returns 0 once it has, -1 on timeout.
 */
int
child_wait(struct akture *a, int kind, int timeout)
{
	struct timespec deadline;

	(void)clock_gettime(CLOCK_MONOTONIC, &deadline);
	deadline.tv_sec += timeout;
	while (child_running(a, kind)) {
		if (timeout >= 0 && ms_left(&deadline) == 0)
			return -1;
		(void)ev_wait(a, timeout < 0 ? -1 : ms_left(&deadline));
	}
	return 0;
}

/*
 * Terminate a child and wait for it.  A session is a process group of
 * its own and is signalled as a whole.
 */
void
child_stop(struct akture *a, int kind)
{
	pid_t pid;

	if (!child_running(a, kind))
		return;
	pid = a->a_child[kind].c_pid;
	if (kind == CHILD_SESSION)
		pid = -pid;

	(void)kill(pid, SIGTERM);
	if (child_wait(a, kind, AK_KILL_TIMEOUT) == 0)
		return;
	syslog(LOG_WARNING, "child %d did not exit, killing it",
	    a->a_child[kind].c_pid);
	(void)kill(pid, SIGKILL);
	(void)child_wait(a, kind, AK_KILL_TIMEOUT);
}
