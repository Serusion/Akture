PROG=	akture-conv
SRCS=	conv.c exif.c image.c jpeg.c png.c
SRCS+=	akbg.c akparse.c akutil.c
MAN=	akture-conv.1
BINDIR=	${AKTURE_PREFIX}/bin

PKGS=	libpng libjpeg
LDADD+=	-lm
DPADD+=	${LIBM}

.if defined(AKTURE_CAPSICUM) && ${AKTURE_CAPSICUM} != "no"
CPPFLAGS+=	-DAKTURE_CAPSICUM
.endif

.include <bsd.prog.mk>
