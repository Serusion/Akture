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
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <xcb/xcb.h>

#if defined(AKTURE_CAPSICUM)
#include <sys/capsicum.h>
#endif

#include "akutil.h"
#include "greeter.h"
#include "pathnames.h"

/*
 * akture-greeter: the login screen.  It runs as the unprivileged
 * greeter user, started by akture(8) with its end of the socketpair on
 * descriptor AKTURE_GREETER_FD, and handles everything that comes from
 * outside: drawing, keyboard, backgrounds and fonts.
 */

static struct greeter greeter;

/*
 * Open the configured faces, falling back to the built-in list.  If
 * no face can be opened at all the greeter cannot work; the master
 * then leaves the console to getty.
 */
static int
fonts_init(struct greeter *g)
{
	static const char *const fallbacks[] = { AKTURE_FONT_FALLBACKS };
	size_t i;

	if (au_font_init() == -1)
		return -1;
	g->g_fui = au_face_open(g->g_opt[AKOPT_FONT_UI]);
	g->g_fclock = au_face_open(g->g_opt[AKOPT_FONT_CLOCK]);
	if (g->g_fui == NULL)
		syslog(LOG_WARNING, "%s: cannot load", g->g_opt[AKOPT_FONT_UI]);
	if (g->g_fclock == NULL)
		syslog(LOG_WARNING, "%s: cannot load",
		    g->g_opt[AKOPT_FONT_CLOCK]);

	for (i = 0; g->g_fui == NULL && g->g_fclock == NULL &&
	    i < __arraycount(fallbacks); i++)
		g->g_fui = au_face_open(fallbacks[i]);
	if (g->g_fui == NULL)
		g->g_fui = g->g_fclock;
	if (g->g_fclock == NULL)
		g->g_fclock = g->g_fui;
	return g->g_fui != NULL ? 0 : -1;
}

/*
 * Give up the ability to open anything new.  On FreeBSD this is
 * capability mode; the greeter keeps only what it opened before.
 */
static void
sandbox_enter(void)
{

#if defined(AKTURE_CAPSICUM)
	if (cap_enter() == -1)
		syslog(LOG_WARNING, "cap_enter: %m");
#endif
}

static void
loop(struct greeter *g)
{
	struct pollfd pfd[2];
	xcb_generic_event_t *ev;
	struct akmsg m;
	int timeout;

	pfd[0].fd = xcb_get_file_descriptor(g->g_x.x_conn);
	pfd[0].events = POLLIN;
	pfd[1].fd = g->g_sock;
	pfd[1].events = POLLIN;

	timeout = ui_tick(g);
	for (;;) {
		ui_paint(g);
		if (poll(pfd, 2, timeout) == -1) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "poll: %m");
			exit(AKTURE_EXIT_ERROR);
		}
		while ((ev = xcb_poll_for_event(g->g_x.x_conn)) != NULL) {
			x11_dispatch(g, ev);
			free(ev);
		}
		if (xcb_connection_has_error(g->g_x.x_conn)) {
			syslog(LOG_ERR, "lost the X server");
			exit(AKTURE_EXIT_ERROR);
		}
		if (pfd[1].revents & (POLLIN | POLLHUP)) {
			if (ipc_recv(g, &m) == -1)
				exit(AKTURE_EXIT_ERROR);
			ui_message(g, &m);
		}
		timeout = ui_tick(g);
	}
}

int
main(int argc, char **argv)
{
	struct greeter *g = &greeter;

	(void)argc;
	(void)argv;
	openlog("akture-greeter", LOG_PID, LOG_AUTH);
	(void)signal(SIGPIPE, SIG_IGN);
	if (geteuid() == 0) {
		syslog(LOG_ERR, "refusing to run as root");
		exit(AKTURE_EXIT_ERROR);
	}

	g->g_sock = AKTURE_GREETER_FD;
	if (ipc_init(g) == -1)
		exit(AKTURE_EXIT_ERROR);
	if (fonts_init(g) == -1) {
		syslog(LOG_ERR, "no usable font");
		exit(AKTURE_EXIT_NOFONT);
	}
	if (x11_init(g) == -1 || kbd_init(g) == -1)
		exit(AKTURE_EXIT_ERROR);
	ui_init(g);
	sandbox_enter();
	loop(g);
	/* NOTREACHED */
	return AKTURE_EXIT_ERROR;
}
