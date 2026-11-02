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

#include <syslog.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * Shut down or reboot on request of the greeter, if allowed.
 */
void
power_exec(struct akture *a, uint32_t action)
{
	static char *const envp[] = {
		__UNCONST("PATH=/bin:/sbin:/usr/bin:/usr/sbin"), NULL
	};
	char *argv[4];

	if (!conf_bool(a, AKOPT_ALLOW_POWER)) {
		syslog(LOG_WARNING, "power action refused by allow_power");
		return;
	}
	if (child_spawn(a, -1) != 0)
		return;

	argv[0] = __UNCONST("shutdown");
	argv[1] = __UNCONST(action == AKPOWER_REBOOT ? "-r" : "-p");
	argv[2] = __UNCONST("now");
	argv[3] = NULL;
	syslog(LOG_NOTICE, "%s requested from the login screen",
	    action == AKPOWER_REBOOT ? "reboot" : "shutdown");
	(void)execve(_PATH_AKTURE_SHUTDOWN, argv, envp);
	syslog(LOG_ERR, "%s: %m", _PATH_AKTURE_SHUTDOWN);
	_exit(127);
}
