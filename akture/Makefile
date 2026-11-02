PROG=	akture
SRCS=	main.c auth.c conf.c dirscan.c event.c greeter.c hook.c \
	lastuser.c power.c session.c state.c xauth.c xserver.c
SRCS+=	akmsg.c akopt.c akparse.c akutil.c
MAN=	akture.8 akture.conf.5
BINDIR=	/usr/sbin

# Machine dependent parts.
OPSYS!=	uname -s
.if ${OPSYS} == "FreeBSD"
SRCS+=	md_freebsd.c
.else
SRCS+=	md_netbsd.c
.endif

LDADD+=	-lpam -lutil
DPADD+=	${LIBPAM} ${LIBUTIL}

.include <bsd.prog.mk>
