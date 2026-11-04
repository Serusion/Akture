# libautuni: the drawing code shared by the programs of the AutumnBSD
# desktop, with the frame buffer of a window in MIT-SHM, the .akbg
# format and its backgrounds, and the parsers of the configuration.
# It is internal: built as a static library and not installed.  The
# master does not link it; it builds akparse.c itself.

LIB=		autuni
SRCS=		akbg.c akparse.c bgmap.c draw.c font.c frame.c

LIBISPRIVATE=	yes		# NetBSD
INTERNALLIB=			# FreeBSD

PKGS=		pixman-1 freetype2 xcb xcb-shm

.include <bsd.lib.mk>
