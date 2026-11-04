# libautuni: the drawing code shared by the programs of the AutumnBSD
# desktop, with the .akbg format and the parsers of the configuration.
# It is internal: built as a static library and not installed.  The
# master does not link it; it builds akparse.c itself.

LIB=		autuni
SRCS=		akbg.c akparse.c draw.c font.c

LIBISPRIVATE=	yes		# NetBSD
INTERNALLIB=			# FreeBSD

PKGS=		pixman-1 freetype2

.include <bsd.lib.mk>
