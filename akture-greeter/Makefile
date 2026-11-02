PROG=	akture-greeter
SRCS=	main.c bg.c draw.c font.c ipc.c kbd.c ui.c x11.c
SRCS+=	akbg.c akmsg.c akopt.c akparse.c akutil.c
MAN=	akture-greeter.8
BINDIR=	/usr/libexec

PKGS=	xcb xcb-shm xcb-randr xcb-xkb pixman-1 freetype2 \
	xkbcommon xkbcommon-x11
LDADD+=	-lm
DPADD+=	${LIBM}

# Build with AKTURE_CAPSICUM=yes on FreeBSD to run in capability mode.
.if defined(AKTURE_CAPSICUM) && ${AKTURE_CAPSICUM} != "no"
CPPFLAGS+=	-DAKTURE_CAPSICUM
.endif

.include <bsd.prog.mk>
