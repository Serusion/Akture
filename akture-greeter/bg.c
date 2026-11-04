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
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "akbg.h"
#include "greeter.h"
#include "pathnames.h"

/*
 * Backgrounds.  For each output the file "name-WxH.akbg" matching its
 * resolution is mapped as it is, by au_bg_map() of libautuni: the
 * pixels are already in the format of the X server, so nothing is
 * decoded or converted.  Outputs without a usable file get the solid
 * background colour, and their resolution is recorded so that a file
 * can be made for it.
 */

static int
bg_has_line(const char *buf, const char *line)
{
	const char *p;

	for (p = buf; (p = strstr(p, line)) != NULL; p++) {
		if (p == buf || p[-1] == '\n')
			return 1;
	}
	return 0;
}

/*
 * Add "WxH" to the list of resolutions lacking a background, once.
 */
static void
bg_record(int w, int h)
{
	char line[32], buf[4096];
	ssize_t n;
	int fd, len;

	len = snprintf(line, sizeof(line), "%dx%d\n", w, h);
	if ((fd = open(_PATH_AKTURE_RESOLUTIONS,
	    O_RDWR | O_APPEND | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0644)) == -1)
		return;
	n = read(fd, buf, sizeof(buf) - 1);
	buf[n > 0 ? n : 0] = '\0';
	if (!bg_has_line(buf, line))
		(void)write(fd, line, (size_t)len);
	(void)close(fd);
}

void
bg_load(struct greeter *g, struct output *o)
{
	const char *name;
	char path[PATH_MAX];
	int fd, w, h0;

	bg_free(o);
	name = g->g_opt[AKOPT_BACKGROUND];
	if (name[0] == '\0')
		return;
	w = o->o_rect.r_w;
	h0 = o->o_rect.r_h;
	if ((size_t)snprintf(path, sizeof(path), "%s/%s-%dx%d%s",
	    _PATH_AKTURE_BGDIR, name, w, h0, AKBG_SUFFIX) >= sizeof(path))
		return;

	if ((fd = open(path, O_RDONLY | O_CLOEXEC)) == -1) {
		if (errno == ENOENT)
			bg_record(w, h0);
		else
			syslog(LOG_WARNING, "%s: %m", path);
		return;
	}
	(void)au_bg_map(&o->o_bg, fd, path, w, h0);
	(void)close(fd);
}

void
bg_free(struct output *o)
{

	au_bg_unmap(&o->o_bg);
}
