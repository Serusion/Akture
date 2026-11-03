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

#ifndef _AKTURE_PATHNAMES_H_
#define _AKTURE_PATHNAMES_H_

/*
 * Every path may be overridden from the build, e.g.
 * CPPFLAGS+= -D_PATH_AKTURE_CONF=\"/usr/pkg/etc/akture/akture.conf\"
 */

#ifndef _PATH_AKTURE_ETC
#define _PATH_AKTURE_ETC	"/etc/akture"
#endif
#ifndef _PATH_AKTURE_CONF
#define _PATH_AKTURE_CONF	_PATH_AKTURE_ETC "/akture.conf"
#endif
#ifndef _PATH_AKTURE_SESSIONS
#define _PATH_AKTURE_SESSIONS	_PATH_AKTURE_ETC "/sessions"
#endif
#ifndef _PATH_AKTURE_HOOKS
#define _PATH_AKTURE_HOOKS	_PATH_AKTURE_ETC "/hooks.d"
#endif

#ifndef _PATH_AKTURE_DB
#define _PATH_AKTURE_DB		"/var/db/akture"
#endif
#define _PATH_AKTURE_LASTUSER	_PATH_AKTURE_DB "/last-user"
#define _PATH_AKTURE_RESOLUTIONS _PATH_AKTURE_DB "/resolutions"

#ifndef _PATH_AKTURE_RUN
#define _PATH_AKTURE_RUN	"/var/run/akture"
#endif
#define _PATH_AKTURE_SERVERAUTH	_PATH_AKTURE_RUN "/server.auth"
#define _PATH_AKTURE_GREETERAUTH _PATH_AKTURE_RUN "/greeter.auth"

#ifndef _PATH_AKTURE_GREETER
#define _PATH_AKTURE_GREETER	"/usr/libexec/akture-greeter"
#endif

#ifndef _PATH_AKTURE_SHARE
#define _PATH_AKTURE_SHARE	"/usr/share/akture"
#endif
#define _PATH_AKTURE_BGDIR	_PATH_AKTURE_SHARE "/backgrounds"
#define _PATH_AKTURE_FONTDIR	_PATH_AKTURE_SHARE "/fonts"

/* Defaults of font_ui and font_clock in akture.conf(5). */
#ifndef _PATH_AKTURE_FONT_UI
#define _PATH_AKTURE_FONT_UI	_PATH_AKTURE_FONTDIR "/Outfit.ttf"
#endif
#ifndef _PATH_AKTURE_FONT_CLOCK
#define _PATH_AKTURE_FONT_CLOCK	_PATH_AKTURE_FONTDIR "/ZenKakuGothicNew-Light.ttf"
#endif

#ifndef _PATH_AKTURE_SHUTDOWN
#define _PATH_AKTURE_SHUTDOWN	"/sbin/shutdown"
#endif

#ifndef _PATH_AKTURE_XSERVER
#if defined(__FreeBSD__)
#define _PATH_AKTURE_XSERVER	"/usr/local/bin/X"
#else
#define _PATH_AKTURE_XSERVER	"/usr/X11R7/bin/X"
#endif
#endif

/* Fonts tried, in order, when the configured ones cannot be loaded. */
#ifndef AKTURE_FONT_FALLBACKS
#define AKTURE_FONT_FALLBACKS						\
	"/usr/X11R7/lib/X11/fonts/TTF/DejaVuSans.ttf",			\
	"/usr/X11R7/lib/X11/fonts/TTF/luxisr.ttf",			\
	"/usr/pkg/share/fonts/X11/TTF/DejaVuSans.ttf",			\
	"/usr/local/share/fonts/dejavu/DejaVuSans.ttf"
#endif

#define AKTURE_USER		"_akture"	/* unprivileged greeter user */
#define AKTURE_PAM_SERVICE	"akture"
#define AKTURE_PAM_AUTOLOGIN	"akture-autologin"
#define AKTURE_DISPLAY		":0"
#define AKTURE_DISPLAYNUM	"0"

#endif /* !_AKTURE_PATHNAMES_H_ */
