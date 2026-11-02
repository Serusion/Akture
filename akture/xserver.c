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
#include <sys/stat.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * The X server.  It is started with a fresh cookie for every run and
 * reports readiness the traditional way: a server started with SIGUSR1
 * ignored sends SIGUSR1 to its parent once it accepts connections.
 */

static __dead void
xserver_exec(struct akture *a)
{
	static char *const envp[] = { __UNCONST("PATH=/usr/bin:/bin"), NULL };
	char args[AKMSG_TEXTMAX + 1];
	char *argv[AK_MAXARGS];
	int argc, n;

	(void)signal(SIGUSR1, SIG_IGN);
	(void)setsid();

	argc = 0;
	argv[argc++] = __UNCONST(conf_str(a, AKOPT_X_SERVER));
	argv[argc++] = __UNCONST(AKTURE_DISPLAY);
	argv[argc++] = __UNCONST("-auth");
	argv[argc++] = __UNCONST(_PATH_AKTURE_SERVERAUTH);
	(void)strlcpy(args, conf_str(a, AKOPT_X_ARGS), sizeof(args));
	if ((n = ak_split_words(args, argv + argc, AK_MAXARGS - argc)) == -1) {
		syslog(LOG_ERR, "x_args: too many arguments");
		_exit(1);
	}
	argc += n;
	argc = md_xserver_args(argv, argc, AK_MAXARGS);
	argv[argc] = NULL;

	(void)execve(argv[0], argv, envp);
	syslog(LOG_ERR, "%s: %m", argv[0]);
	_exit(127);
}

/*
 * Start the X server and wait until it is ready.
 */
int
xserver_start(struct akture *a)
{
	time_t deadline;
	pid_t pid;
	int left;

	arc4random_buf(a->a_cookie, sizeof(a->a_cookie));
	if (mkdir(_PATH_AKTURE_RUN, 0711) == -1 && errno != EEXIST) {
		syslog(LOG_ERR, "%s: %m", _PATH_AKTURE_RUN);
		return -1;
	}
	if (xauth_write(_PATH_AKTURE_SERVERAUTH, a->a_cookie, 0, 0) == -1 ||
	    xauth_write(_PATH_AKTURE_GREETERAUTH, a->a_cookie,
	    a->a_gcred.ac_uid, a->a_gcred.ac_gid) == -1) {
		syslog(LOG_ERR, "cannot write the X authority: %m");
		return -1;
	}

	if ((pid = child_spawn(a, CHILD_X)) == -1)
		return -1;
	if (pid == 0)
		xserver_exec(a);

	deadline = time(NULL) + AK_XSTART_TIMEOUT;
	while ((left = (int)(deadline - time(NULL))) > 0) {
		switch (ev_wait(a, left * 1000)) {
		case EV_XREADY:
			syslog(LOG_DEBUG, "X server ready");
			return 0;
		case EV_QUIT:
			xserver_stop(a);
			return -1;
		case EV_CHILD:
			if (!child_running(a, CHILD_X)) {
				syslog(LOG_ERR, "X server exited at startup");
				return -1;
			}
			break;
		default:
			break;
		}
	}
	syslog(LOG_ERR, "X server did not become ready");
	xserver_stop(a);
	return -1;
}

void
xserver_stop(struct akture *a)
{

	child_stop(a, CHILD_X);
	(void)unlink(_PATH_AKTURE_GREETERAUTH);
	(void)unlink(_PATH_AKTURE_SERVERAUTH);
	ak_explicit_zero(a->a_cookie, sizeof(a->a_cookie));
}
