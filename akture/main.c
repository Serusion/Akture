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
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * akture: the privileged master of the display manager.  It runs the
 * X server, the greeter and the sessions, and is the only part that
 * talks to PAM.  See akture(8).
 */

/* What ended the idle loop. */
#define IDLE_SESSION	0	/* a user was authenticated */
#define IDLE_XDIED	1	/* restart the X server */
#define IDLE_QUIT	2	/* asked to terminate */
#define IDLE_FATAL	3	/* leave the console to getty */

static struct akture akture;

static __dead void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-d] [-f config]\n", getprogname());
	exit(EXIT_FAILURE);
}

static void
transition(struct akture *a, int ev)
{

	if (akstate_next(a->a_state, ev, &a->a_state) == -1) {
		/* Cannot happen; refuse to go on in an unknown state. */
		syslog(LOG_CRIT, "invalid event %d in state %d", ev,
		    a->a_state);
		abort();
	}
}

/*
 * Sleep up to sec seconds while still noticing signals.  Returns -1
 * when asked to quit.
 */
static int
pause_for(struct akture *a, unsigned int sec)
{
	time_t deadline;
	int left;

	deadline = time(NULL) + sec;
	while ((left = (int)(deadline - time(NULL))) > 0) {
		if (ev_wait(a, left * 1000) == EV_QUIT)
			return -1;
	}
	return a->a_quit ? -1 : 0;
}

/*
 * Start the X server, trying a few times with growing pauses.
 */
static int
start_x(struct akture *a)
{
	int i;

	for (i = 0; i < AK_XSTART_TRIES; i++) {
		if (i > 0 && pause_for(a, 2U << i) == -1)
			return -1;
		if (xserver_start(a) == 0)
			return 0;
		if (a->a_quit)
			return -1;
	}
	syslog(LOG_ERR, "giving up on the X server");
	return -1;
}

/*
 * Record a greeter failure.  Returns -1 if it fails too often, in
 * which case the console is left to getty.
 */
static int
greeter_fault(struct akture *a)
{
	time_t now;
	int status;

	greeter_stop(a);
	status = a->a_child[CHILD_GREETER].c_status;
	if (WIFEXITED(status) && WEXITSTATUS(status) == AKTURE_EXIT_NOFONT) {
		syslog(LOG_ERR, "greeter has no usable font");
		return -1;
	}

	now = time(NULL);
	if (now - a->a_gfailstart > AK_GREETER_FAILWIN) {
		a->a_gfailstart = now;
		a->a_gfails = 0;
	}
	if (++a->a_gfails > AK_GREETER_FAILMAX) {
		syslog(LOG_ERR, "greeter keeps failing");
		return -1;
	}
	syslog(LOG_WARNING, "greeter failed (%d)", a->a_gfails);
	return 0;
}

/*
 * (Re)start the greeter, backing off after failures.
 */
static int
greeter_respawn(struct akture *a)
{
	char last[AKMSG_NAMEMAX + 1];

	for (;;) {
		if (a->a_gfails > 0 &&
		    pause_for(a, 1U << (a->a_gfails - 1)) == -1)
			return IDLE_QUIT;
		if (!child_running(a, CHILD_X))
			return IDLE_XDIED;
		if (greeter_start(a) == 0)
			break;
		if (greeter_fault(a) == -1)
			return IDLE_FATAL;
	}
	if (!a->a_shown) {
		a->a_shown = 1;
		if (lastuser_read(last, sizeof(last)) == -1)
			last[0] = '\0';
		hook_run(a, "greeter-shown", last, NULL);
	}
	return -1;
}

/*
 * Authenticate the user of a BEGIN_AUTH message.  Returns 1 if the
 * session may start, 0 to stay idle, -1 if the greeter misbehaved.
 */
static int
do_auth(struct akture *a, struct akmsg *m)
{

	if (m->am_args[0] >= (uint32_t)a->a_nsessions)
		return -1;
	transition(a, AKE_BEGIN_AUTH);

	switch (auth_greeter(a, m)) {
	case AUTH_OK:
		transition(a, AKE_AUTH_OK);
		a->a_nfail = 0;
		(void)greeter_result(a, AKRESULT_OK);
		greeter_stop(a);
		hook_run(a, "auth-ok", a->a_user,
		    a->a_sessions[a->a_session].s_id);
		return 1;
	case AUTH_FAIL:
		/* Delay the answer; the greeter stays busy meanwhile. */
		a->a_nfail++;
		(void)ev_watch(a, a->a_gsock, 0);
		(void)pause_for(a, akstate_fail_delay(a->a_nfail));
		(void)ev_watch(a, a->a_gsock, 1);
		transition(a, AKE_AUTH_FAIL);
		return greeter_result(a, AKRESULT_FAIL) == -1 ? -1 : 0;
	case AUTH_CANCEL:
		transition(a, AKE_AUTH_FAIL);
		return greeter_result(a, AKRESULT_FAIL) == -1 ? -1 : 0;
	default:
		transition(a, AKE_AUTH_FAIL);
		return -1;
	}
}

/*
 * Show the greeter and serve it until someone is authenticated.
 */
static int
idle(struct akture *a)
{
	struct akmsg m;
	int rv;

	for (;;) {
		if (a->a_quit)
			return IDLE_QUIT;
		if (!child_running(a, CHILD_X)) {
			greeter_stop(a);
			return IDLE_XDIED;
		}
		if (a->a_gsock == -1 && (rv = greeter_respawn(a)) != -1)
			return rv;

		switch (ev_wait(a, -1)) {
		case EV_READ:
			if (greeter_recv(a, &m) == -1 ||
			    !akstate_accepts(a->a_state, m.am_type)) {
				akmsg_clear(&m);
				if (greeter_fault(a) == -1)
					return IDLE_FATAL;
				break;
			}
			switch (m.am_type) {
			case AKMSG_BEGIN_AUTH:
				rv = do_auth(a, &m);
				akmsg_clear(&m);
				if (rv == 1)
					return IDLE_SESSION;
				if (rv == -1 && greeter_fault(a) == -1)
					return IDLE_FATAL;
				break;
			case AKMSG_POWER:
				transition(a, AKE_POWER);
				power_exec(a, m.am_args[0]);
				break;
			case AKMSG_CANCEL:
				/* Crossed the result of the attempt. */
				transition(a, AKE_CANCEL);
				break;
			}
			break;
		case EV_CHILD:
			if (a->a_gsock != -1 &&
			    !child_running(a, CHILD_GREETER) &&
			    greeter_fault(a) == -1)
				return IDLE_FATAL;
			break;
		case EV_QUIT:
			return IDLE_QUIT;
		default:
			break;
		}
	}
}

/*
 * Run the session of the authenticated user, then reset the X server
 * by restarting it, so that no client of the session survives.
 */
static void
do_session(struct akture *a)
{

	lastuser_write(a->a_user);
	if (session_run(a) == -1)
		a->a_initflags |= AKINIT_SESSIONFAIL;
	transition(a, AKE_SESSION_END);
	xserver_stop(a);
}

static void
do_autologin(struct akture *a)
{
	const char *user;
	int s;

	user = conf_str(a, AKOPT_AUTOLOGIN_USER);
	if ((s = session_lookup(a, conf_str(a, AKOPT_DEFAULT_SESSION))) == -1)
		s = 0;
	a->a_session = (uint32_t)s;

	transition(a, AKE_BEGIN_AUTH);
	if (auth_autologin(a, user) != AUTH_OK) {
		syslog(LOG_WARNING, "autologin of %s failed", user);
		transition(a, AKE_AUTH_FAIL);
		return;
	}
	transition(a, AKE_AUTH_OK);
	syslog(LOG_INFO, "autologin of %s", user);
	/* Keep the order of the hooks the same as for a normal login. */
	hook_run(a, "greeter-shown", user, NULL);
	a->a_shown = 1;
	hook_run(a, "auth-ok", user, a->a_sessions[s].s_id);
	do_session(a);
}

static int
run(struct akture *a)
{
	int autologin;

	autologin = conf_str(a, AKOPT_AUTOLOGIN_USER)[0] != '\0';
	for (;;) {
		if (!child_running(a, CHILD_X) && start_x(a) == -1)
			return a->a_quit ? EXIT_SUCCESS : EXIT_FAILURE;
		if (autologin) {
			/* Only once, at boot; after logout the greeter runs. */
			autologin = 0;
			do_autologin(a);
			continue;
		}
		switch (idle(a)) {
		case IDLE_SESSION:
			do_session(a);
			break;
		case IDLE_XDIED:
			syslog(LOG_WARNING, "X server died, restarting it");
			break;
		case IDLE_QUIT:
			return EXIT_SUCCESS;
		default:
			return EXIT_FAILURE;
		}
	}
}

int
main(int argc, char **argv)
{
	struct akture *a = &akture;
	const char *conffile;
	int ch, rv;

	conffile = _PATH_AKTURE_CONF;
	while ((ch = getopt(argc, argv, "df:")) != -1) {
		switch (ch) {
		case 'd':
			a->a_debug = 1;
			break;
		case 'f':
			conffile = optarg;
			break;
		default:
			usage();
		}
	}
	if (optind != argc)
		usage();
	if (geteuid() != 0)
		errx(EXIT_FAILURE, "must be run as root");

	openlog(getprogname(), LOG_PID | (a->a_debug ? LOG_PERROR : 0),
	    LOG_AUTH);
	a->a_state = AKS_IDLE;
	a->a_gsock = -1;
	(void)conf_load(&a->a_conf, conffile);
	(void)sessions_load(a);
	if (ak_cred_user(AKTURE_USER, &a->a_gcred) == -1 ||
	    a->a_gcred.ac_uid == 0) {
		syslog(LOG_ERR, "no usable user %s", AKTURE_USER);
		exit(EXIT_FAILURE);
	}
	/* The greeter gets no supplementary groups. */
	a->a_gcred.ac_ngroups = 1;
	a->a_gcred.ac_groups[0] = a->a_gcred.ac_gid;
	if (mkdir(_PATH_AKTURE_DB, 0755) == -1 && errno != EEXIST)
		syslog(LOG_WARNING, "%s: %m", _PATH_AKTURE_DB);

	if (!a->a_debug && daemon(0, 0) == -1) {
		syslog(LOG_ERR, "daemon: %m");
		exit(EXIT_FAILURE);
	}
	if (ev_init(a) == -1) {
		syslog(LOG_ERR, "cannot set up events: %m");
		exit(EXIT_FAILURE);
	}

	rv = run(a);

	greeter_stop(a);
	child_stop(a, CHILD_SESSION);
	xserver_stop(a);
	md_console();
	syslog(LOG_INFO, "exiting");
	return rv;
}
