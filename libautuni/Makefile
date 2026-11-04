# libautuni: the drawing code shared by the programs of the AutumnBSD
# desktop, with the frame buffer of a window in MIT-SHM, the .akbg
# background format and the parsers of the configuration.  See
# README.md.
#
# "make install" installs a static library, its headers in
# include/autuni and autuni.pc for pkg-config, under AUTUNI_PREFIX.
# A project that builds the library as a part of itself, as Akture
# does, sets AUTUNI_INTERNAL in its Makefile.inc: the library is then
# built, and nothing is installed.

.include <bsd.init.mk>

AUTUNI_VERSION=	0.0.20261104
AUTUNI_PREFIX?=	/usr/local

LIB=		autuni
SRCS=		akbg.c akparse.c bgmap.c draw.c font.c frame.c

WARNS?=		5
CFLAGS+=	-std=c11
CPPFLAGS+=	-D_NETBSD_SOURCE
AUTUNI_PKGS=	pixman-1 freetype2 xcb xcb-shm
AUTUNI_CFLAGS!=	pkg-config --cflags ${AUTUNI_PKGS}
CPPFLAGS+=	${AUTUNI_CFLAGS}

.if defined(AUTUNI_INTERNAL)
LIBISPRIVATE=	yes		# NetBSD
INTERNALLIB=			# FreeBSD
.else
# A static library and nothing else.
MKPIC=		no
MKPROFILE=	no
MKLINT=		no
MKDEBUGLIB=	no

LIBDIR=		${AUTUNI_PREFIX}/lib
INCS=		autuni.h akbg.h akparse.h
INCSDIR=	${AUTUNI_PREFIX}/include/autuni
FILES=		autuni.pc
FILESDIR=	${AUTUNI_PREFIX}/lib/pkgconfig
FILESBUILD_autuni.pc=	yes
CLEANFILES+=	autuni.pc

autuni.pc: autuni.pc.in
	sed -e 's,@PREFIX@,${AUTUNI_PREFIX},g' \
	    -e 's,@VERSION@,${AUTUNI_VERSION},g' ${.ALLSRC} > ${.TARGET}
.endif

.include <bsd.lib.mk>

.if !defined(AUTUNI_INTERNAL) && target(incinstall)
# NetBSD installs headers with "make includes"; "make install" too.
realinstall: incinstall
.endif
