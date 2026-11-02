# Unit tests (atf-c).  Run with "make && kyua test" or atf-run.

TESTSDIR=	/usr/tests/akture

TESTS_C=	t_akbg t_akmsg t_conf t_exif t_state t_xauth

SRCS.t_akbg=	t_akbg.c akbg.c
SRCS.t_akmsg=	t_akmsg.c akmsg.c akutil.c
SRCS.t_conf=	t_conf.c conf.c akopt.c akparse.c
SRCS.t_exif=	t_exif.c exif.c
SRCS.t_state=	t_state.c state.c
SRCS.t_xauth=	t_xauth.c xauth.c akutil.c

.PATH:		${.CURDIR}/../common ${.CURDIR}/../akture \
		${.CURDIR}/../akture-conv
CPPFLAGS+=	-D_NETBSD_SOURCE -I${.CURDIR}/../common \
		-I${.CURDIR}/../akture -I${.CURDIR}/../akture-conv

.include <bsd.test.mk>
