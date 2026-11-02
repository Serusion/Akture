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
#include <sys/time.h>

#include <security/pam_appl.h>

#include <errno.h>
#include <limits.h>
#include <login_cap.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmpx.h>

#include "akture.h"
#include "pathnames.h"

/*
 * Sessions: the list in /etc/akture/sessions, and running one for an
 * authenticated user.
 */

#define SESSION_SHELL		"/bin/sh"
#define SESSION_XSESSION	".xsession"
#define SESSION_XAUTHORITY	".Xauthority"
#define SESSION_SCANMAX		64

extern char **environ;

/*
 * Read one session file: "name = ..." and "exec = ...".
 */
static int
session_read(const char *file, struct session *s)
{
	char line[AKPARSE_LINEMAX], path[PATH_MAX];
	char key[AKPARSE_LINEMAX], val[AKPARSE_LINEMAX];
	FILE *fp;

	(void)snprintf(path, sizeof(path), "%s/%s", _PATH_AKTURE_SESSIONS,
	    file);
	if ((fp = fopen(path, "re")) == NULL) {
		syslog(LOG_WARNING, "%s: %m", path);
		return -1;
	}
	(void)memset(s, 0, sizeof(*s));
	(void)strlcpy(s->s_id, file, sizeof(s->s_id));
	while (fgets(line, sizeof(line), fp) != NULL) {
		if (ak_parse_line(line, key, val) != AKPARSE_PAIR)
			continue;
		if (strcmp(key, "name") == 0)
			(void)strlcpy(s->s_name, val, sizeof(s->s_name));
		else if (strcmp(key, "exec") == 0)
			(void)strlcpy(s->s_exec, val, sizeof(s->s_exec));
		else
			syslog(LOG_WARNING, "%s: unknown key %s", path, key);
	}
	(void)fclose(fp);

	if (s->s_name[0] == '\0' || s->s_exec[0] != '/') {
		syslog(LOG_WARNING, "%s: needs a name and an absolute exec",
		    path);
		return -1;
	}
	return 0;
}

int
sessions_load(struct akture *a)
{
	char names[SESSION_SCANMAX][NAME_MAX + 1];
	int i, n;

	a->a_nsessions = 0;
	if ((n = dir_scan(_PATH_AKTURE_SESSIONS, names, SESSION_SCANMAX)) ==
	    -1)
		syslog(LOG_WARNING, "%s: %m", _PATH_AKTURE_SESSIONS);
	for (i = 0; i < n && a->a_nsessions < AK_MAXSESSIONS; i++) {
		if (session_read(names[i], &a->a_sessions[a->a_nsessions]) ==
		    0)
			a->a_nsessions++;
	}

	if (a->a_nsessions == 0) {
		/* Fall back to the user's ~/.xsession. */
		syslog(LOG_WARNING, "no sessions, offering ~/%s only",
		    SESSION_XSESSION);
		(void)memset(&a->a_sessions[0], 0, sizeof(a->a_sessions[0]));
		(void)strlcpy(a->a_sessions[0].s_id, "xsession",
		    sizeof(a->a_sessions[0].s_id));
		(void)strlcpy(a->a_sessions[0].s_name, "Xsession",
		    sizeof(a->a_sessions[0].s_name));
		a->a_nsessions = 1;
	}
	return 0;
}

/*
 * Find a session by file name or displayed name.
 */
int
session_lookup(const struct akture *a, const char *name)
{
	int i;

	if (name[0] == '\0')
		return -1;
	for (i = 0; i < a->a_nsessions; i++) {
		if (strcmp(a->a_sessions[i].s_id, name) == 0 ||
		    strcmp(a->a_sessions[i].s_name, name) == 0)
			return i;
	}
	return -1;
}

static void
session_utmpx(const char *user, pid_t pid, short type)
{
	struct utmpx ut;

	(void)memset(&ut, 0, sizeof(ut));
	ut.ut_type = type;
	ut.ut_pid = pid;
	(void)strncpy(ut.ut_user, user, sizeof(ut.ut_user));
	(void)strncpy(ut.ut_id, "ak" AKTURE_DISPLAYNUM, sizeof(ut.ut_id));
	(void)strncpy(ut.ut_line, AKTURE_DISPLAY, sizeof(ut.ut_line));
	(void)strncpy(ut.ut_host, AKTURE_DISPLAY, sizeof(ut.ut_host));
	(void)gettimeofday(&ut.ut_tv, NULL);

	setutxent();
	if (pututxline(&ut) == NULL)
		syslog(LOG_WARNING, "pututxline: %m");
	endutxent();
#if defined(__NetBSD__)
	(void)updwtmpx(_PATH_WTMPX, &ut);
#endif
}

/*
 * Give the session its X authority.  Written as the user, after the
 * privileges are gone, so that no link in the home directory can
 * redirect a write made as root.
 */
static void
session_xauth(const struct akture *a, const char *home)
{
	char path[PATH_MAX];
	int fd;

	if ((size_t)snprintf(path, sizeof(path), "%s/%s", home,
	    SESSION_XAUTHORITY) < sizeof(path) &&
	    xauth_merge(path, a->a_cookie) == 0) {
		(void)setenv("XAUTHORITY", path, 1);
		return;
	}

	/* Read-only or full home directory: use a private file. */
	(void)strlcpy(path, "/tmp/.akture-xauth-XXXXXX", sizeof(path));
	if ((fd = mkstemp(path)) != -1) {
		(void)close(fd);
		if (xauth_write(path, a->a_cookie, getuid(), getgid()) == 0) {
			(void)setenv("XAUTHORITY", path, 1);
			return;
		}
	}
	syslog(LOG_WARNING, "cannot write an X authority for %s", a->a_user);
}

/*
 * Build the command of the session in argv, using buf for storage.
 */
static int
session_argv(const struct akture *a, const struct session *s,
    const char *home, char *buf, size_t len, char **argv)
{
	char xsession[PATH_MAX];

	(void)snprintf(xsession, sizeof(xsession), "%s/%s", home,
	    SESSION_XSESSION);
	if (s->s_exec[0] == '\0' ||
	    (conf_bool(a, AKOPT_PREFER_XSESSION) &&
	    access(xsession, R_OK) == 0)) {
		(void)strlcpy(buf, xsession, len);
		argv[0] = __UNCONST(SESSION_SHELL);
		argv[1] = buf;
		argv[2] = NULL;
		return 0;
	}
	(void)strlcpy(buf, s->s_exec, len);
	return ak_split_words(buf, argv, AK_MAXARGS) > 0 ? 0 : -1;
}

/*
 * The session child.  Applies the login class, drops privileges and
 * executes the session; it never returns.  Any failure on the way
 * ends the child without running anything as the user.
 */
static __dead void
session_exec(struct akture *a)
{
	static char *envinit[1];
	char buf[AKPARSE_LINEMAX], home[PATH_MAX];
	char *argv[AK_MAXARGS];
	char **penv;
	struct akcred cred;
	struct passwd *pw;
	login_cap_t *lc;
	const struct session *s;

	s = &a->a_sessions[a->a_session];
	if (setsid() == -1)
		_exit(1);
	if ((pw = getpwnam(a->a_user)) == NULL ||
	    ak_cred_user(a->a_user, &cred) == -1) {
		syslog(LOG_ERR, "%s: no such user", a->a_user);
		_exit(1);
	}
	(void)strlcpy(home, pw->pw_dir, sizeof(home));

	/* Login class: resource limits, umask, PATH and environment. */
	environ = envinit;
	lc = login_getpwclass(pw);
	if (setusercontext(lc, pw, pw->pw_uid,
	    LOGIN_SETALL & ~(LOGIN_SETGROUP | LOGIN_SETUSER)) == -1) {
		syslog(LOG_ERR, "setusercontext: %m");
		_exit(1);
	}
	login_close(lc);
	if ((penv = pam_getenvlist(a->a_pamh)) != NULL) {
		for (; *penv != NULL; penv++)
			(void)putenv(*penv);
	}
	(void)setenv("HOME", home, 1);
	(void)setenv("SHELL", pw->pw_shell, 1);
	(void)setenv("USER", pw->pw_name, 1);
	(void)setenv("LOGNAME", pw->pw_name, 1);
	(void)setenv("DISPLAY", AKTURE_DISPLAY, 1);

	/* setgroups, setgid, setuid; stop at the first failure. */
	if (ak_privdrop(&cred) == -1) {
		syslog(LOG_ERR, "cannot drop privileges for %s", a->a_user);
		_exit(1);
	}

	if (chdir(home) == -1) {
		syslog(LOG_WARNING, "%s: %m", home);
		(void)chdir("/");
	}
	session_xauth(a, home);
	if (session_argv(a, s, home, buf, sizeof(buf), argv) == -1) {
		syslog(LOG_ERR, "session %s: bad command", s->s_id);
		_exit(1);
	}
	(void)execve(argv[0], argv, environ);
	syslog(LOG_ERR, "%s: %m", argv[0]);
	_exit(127);
}

/*
 * Run the session of the authenticated user and wait for it to end.
 * Returns -1 if it could not be started or ended at once.
 */
/*@ requires a->a_state == AKS_SESSION; */
int
session_run(struct akture *a)
{
	char user[AKMSG_NAMEMAX + 1];
	const char *id;
	time_t start;
	pid_t pid;
	int rv;

	if (a->a_state != AKS_SESSION || a->a_pamh == NULL)
		return -1;
	(void)strlcpy(user, a->a_user, sizeof(user));
	id = a->a_sessions[a->a_session].s_id;

	if ((rv = pam_setcred(a->a_pamh, PAM_ESTABLISH_CRED)) !=
	    PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_setcred: %s", pam_strerror(a->a_pamh, rv));
		auth_end(a, rv);
		return -1;
	}
	if ((rv = pam_open_session(a->a_pamh, 0)) != PAM_SUCCESS) {
		syslog(LOG_ERR, "pam_open_session: %s",
		    pam_strerror(a->a_pamh, rv));
		(void)pam_setcred(a->a_pamh, PAM_DELETE_CRED);
		auth_end(a, rv);
		return -1;
	}

	start = time(NULL);
	if ((pid = child_spawn(a, CHILD_SESSION)) == 0)
		session_exec(a);
	if (pid != -1) {
		syslog(LOG_INFO, "session %s started for %s", id, user);
		session_utmpx(user, pid, USER_PROCESS);
		hook_run(a, "session-started", user, id);
		while (child_running(a, CHILD_SESSION)) {
			if (ev_wait(a, -1) == EV_QUIT)
				child_stop(a, CHILD_SESSION);
		}
		session_utmpx(user, pid, DEAD_PROCESS);
		syslog(LOG_INFO, "session of %s ended", user);
	}

	(void)pam_close_session(a->a_pamh, 0);
	(void)pam_setcred(a->a_pamh, PAM_DELETE_CRED);
	auth_end(a, PAM_SUCCESS);
	hook_run(a, "session-ended", user, NULL);

	if (pid == -1 || time(NULL) - start < AK_SESSION_MINTIME)
		return -1;
	return 0;
}
