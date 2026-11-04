# Unit tests (atf-c) of libautuni.  Build the library first, then run
# them with "make && kyua test", or each test program by hand.
# AUTUNI_TEST_FONT names a font for the text of t_font.

TESTSDIR=	/usr/tests/libautuni

TESTS_C=	t_akbg t_akparse t_bgmap t_draw t_font t_frame

WARNS?=		5
CFLAGS+=	-std=c11
CPPFLAGS+=	-D_NETBSD_SOURCE -I${.CURDIR}/..

TEST_PKGS=	pixman-1 freetype2 xcb xcb-shm
TEST_CFLAGS!=	pkg-config --cflags ${TEST_PKGS}
TEST_LIBS!=	pkg-config --libs ${TEST_PKGS}
LIBAUTUNI!=	cd ${.CURDIR}/.. && ${MAKE} -V .OBJDIR
CPPFLAGS+=	${TEST_CFLAGS}
LDADD+=		-L${LIBAUTUNI} -lautuni ${TEST_LIBS} -lm
DPADD+=		${LIBAUTUNI}/libautuni.a

.include <bsd.test.mk>
