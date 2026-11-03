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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * Hooks: the executables in /etc/akture/hooks.d are run in name order
 * at fixed points.  They run as root, so only files that root owns and
 * nobody else can write, in a directory with the same property, are
 * considered.  A separate runner process executes them one after the
 * other with a time limit; the master never waits for it.
 */

static int
hook_safe(const char *path, int dir)
{
	struct stat st;

	if (lstat(path, &st) == -1)
		return 0;
	if (dir ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode))
		return 0;
	if (st.st_uid != 0 || (st.st_mode & (S_IWGRP | S_IWOTH)) != 0)
		return 0;
	return dir || (st.st_mode & S_IXUSR) != 0;
}

/*
 * Run one hook and wait for it, at most AK_HOOK_TIMEOUT seconds.
 * SIGCHLD is blocked by the caller.
 */
static void
hook_exec(const char *path, char **argv, const sigset_t *chld)
{
	static char *const envp[] = {
		__UNCONST("PATH=/bin:/sbin:/usr/bin:/usr/sbin"), NULL
	};
	struct timespec ts;
	pid_t pid;
	int fd, status;

	if ((pid = fork()) == -1) {
		syslog(LOG_ERR, "hook %s: fork: %m", path);
		return;
	}
	if (pid == 0) {
		(void)setpgid(0, 0);
		if ((fd = open(_PATH_DEVNULL, O_RDWR)) != -1) {
			(void)dup2(fd, STDIN_FILENO);
			(void)dup2(fd, STDOUT_FILENO);
			(void)dup2(fd, STDERR_FILENO);
		}
		closefrom(STDERR_FILENO + 1);
		(void)sigprocmask(SIG_UNBLOCK, chld, NULL);
		(void)execve(path, argv, envp);
		_exit(127);
	}

	ts.tv_sec = AK_HOOK_TIMEOUT;
	ts.tv_nsec = 0;
	while (waitpid(pid, &status, WNOHANG) == 0) {
		if (sigtimedwait(chld, NULL, &ts) == -1 && errno == EAGAIN) {
			syslog(LOG_WARNING, "hook %s timed out", path);
			(void)kill(-pid, SIGKILL);
			(void)waitpid(pid, &status, 0);
			return;
		}
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		syslog(LOG_WARNING, "hook %s failed (status %d)", path, status);
}

/*
 * Run the hooks for event with up to two arguments.  NULL arguments
 * are left out; an empty one is passed as "".
 */
void
hook_run(struct akture *a, const char *event, const char *arg1,
    const char *arg2)
{
	char names[AK_MAXHOOKS][NAME_MAX + 1];
	char path[PATH_MAX];
	char *argv[5];
	sigset_t chld;
	int i, n, argc;

	if (child_spawn(a, -1) != 0)
		return;

	/* The runner. */
	(void)sigemptyset(&chld);
	(void)sigaddset(&chld, SIGCHLD);
	(void)sigprocmask(SIG_BLOCK, &chld, NULL);
	if (!hook_safe(_PATH_AKTURE_HOOKS, 1)) {
		if (access(_PATH_AKTURE_HOOKS, F_OK) == 0)
			syslog(LOG_WARNING, "%s: unsafe permissions, "
			    "hooks disabled", _PATH_AKTURE_HOOKS);
		_exit(0);
	}
	if ((n = dir_scan(_PATH_AKTURE_HOOKS, names, AK_MAXHOOKS)) == -1)
		_exit(0);

	for (i = 0; i < n; i++) {
		if ((size_t)snprintf(path, sizeof(path), "%s/%s",
		    _PATH_AKTURE_HOOKS, names[i]) >= sizeof(path)) {
			syslog(LOG_WARNING, "hook %s: name too long", names[i]);
			continue;
		}
		if (!hook_safe(path, 0)) {
			syslog(LOG_WARNING, "hook %s: not a root owned, "
			    "executable file writable only by root", path);
			continue;
		}
		argc = 0;
		argv[argc++] = path;
		argv[argc++] = __UNCONST(event);
		if (arg1 != NULL)
			argv[argc++] = __UNCONST(arg1);
		if (arg2 != NULL)
			argv[argc++] = __UNCONST(arg2);
		argv[argc] = NULL;
		hook_exec(path, argv, &chld);
	}
	_exit(0);
}
