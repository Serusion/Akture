PROG=	akture-greeter
SRCS=	main.c bg.c ipc.c kbd.c ui.c x11.c
SRCS+=	akmsg.c akopt.c akutil.c
MAN=	akture-greeter.8
BINDIR=	${AKTURE_PREFIX}/libexec

PKGS=	xcb xcb-shm xcb-randr xcb-xkb pixman-1 freetype2 \
	xkbcommon xkbcommon-x11
LIBAUTUNI!=	cd ${.CURDIR}/../libautuni && ${MAKE} -V .OBJDIR
LDADD+=	-L${LIBAUTUNI} -lautuni -lm
DPADD+=	${LIBAUTUNI}/libautuni.a ${LIBM}

# Build with AKTURE_CAPSICUM=yes on FreeBSD to run in capability mode.
.if defined(AKTURE_CAPSICUM) && ${AKTURE_CAPSICUM} != "no"
CPPFLAGS+=	-DAKTURE_CAPSICUM
.endif

.include <bsd.prog.mk>
