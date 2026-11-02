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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "akture.h"
#include "pathnames.h"

/*
 * The last user to log in, kept in a file only root can read.  The
 * greeter gets the name in AKMSG_INIT, and the greeter-shown hook as
 * its argument.
 */

int
lastuser_read(char *buf, size_t len)
{
	ssize_t n;
	int fd;

	buf[0] = '\0';
	if ((fd = open(_PATH_AKTURE_LASTUSER,
	    O_RDONLY | O_NOFOLLOW | O_CLOEXEC)) == -1)
		return -1;
	n = read(fd, buf, len - 1);
	(void)close(fd);
	if (n <= 0) {
		buf[0] = '\0';
		return -1;
	}
	buf[n] = '\0';
	buf[strcspn(buf, "\n")] = '\0';
	if (!akopt_valid(AKOPT_AUTOLOGIN_USER, buf)) {
		/* Same rules as any other user name in the configuration. */
		buf[0] = '\0';
		return -1;
	}
	return 0;
}

void
lastuser_write(const char *user)
{
	char buf[AKMSG_NAMEMAX + 2];
	int len;

	len = snprintf(buf, sizeof(buf), "%s\n", user);
	if (len < 0 || (size_t)len >= sizeof(buf))
		return;
	if (ak_write_file(_PATH_AKTURE_LASTUSER, buf, (size_t)len, 0600, 0, 0)
	    == -1)
		syslog(LOG_WARNING, "%s: %m", _PATH_AKTURE_LASTUSER);
}
