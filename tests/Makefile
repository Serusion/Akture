# Unit tests (atf-c).  Run with "make && kyua test" or atf-run.

TESTSDIR=	/usr/tests/akture

TESTS_C=	t_akbg t_akmsg t_conf t_exif t_render t_state t_xauth

SRCS.t_akbg=	t_akbg.c
SRCS.t_akmsg=	t_akmsg.c akmsg.c akutil.c
SRCS.t_conf=	t_conf.c conf.c akopt.c akparse.c
SRCS.t_exif=	t_exif.c exif.c
SRCS.t_render=	t_render.c ui.c akmsg.c akopt.c akutil.c
SRCS.t_state=	t_state.c state.c
SRCS.t_xauth=	t_xauth.c xauth.c akutil.c

.PATH:		${.CURDIR}/../common ${.CURDIR}/../libautuni \
		${.CURDIR}/../akture ${.CURDIR}/../akture-conv \
		${.CURDIR}/../akture-greeter
CPPFLAGS+=	-D_NETBSD_SOURCE -I${.CURDIR}/../common \
		-I${.CURDIR}/../libautuni -I${.CURDIR}/../akture \
		-I${.CURDIR}/../akture-conv -I${.CURDIR}/../akture-greeter

# t_akbg and t_render link libautuni, as akture-greeter does; build it
# first.  t_conf builds akparse.c itself, as the master does.
LIBAUTUNI!=	cd ${.CURDIR}/../libautuni && ${MAKE} -V .OBJDIR
LDADD.t_akbg+=	-L${LIBAUTUNI} -lautuni
DPADD.t_akbg+=	${LIBAUTUNI}/libautuni.a

# t_render draws with the code of akture-greeter, whose headers need
# those of xcb and xkbcommon too; only pixman and FreeType are linked.
RENDER_CFLAGS!=	pkg-config --cflags pixman-1 freetype2 xcb xkbcommon
RENDER_LIBS!=	pkg-config --libs pixman-1 freetype2
CPPFLAGS+=	${RENDER_CFLAGS}
LDADD.t_render+=	-L${LIBAUTUNI} -lautuni ${RENDER_LIBS} -lm
DPADD.t_render+=	${LIBAUTUNI}/libautuni.a

.include <bsd.test.mk>
