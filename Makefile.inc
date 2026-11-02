# Settings shared by all programs.

WARNS?=		5
CFLAGS+=	-std=c11
CPPFLAGS+=	-D_NETBSD_SOURCE -I${.CURDIR}/../common
.PATH:		${.CURDIR}/../common

# Flags of the pkg-config(1) packages listed in PKGS by a program.
.if defined(PKGS) && !empty(PKGS)
PKGS_CFLAGS!=	pkg-config --cflags ${PKGS}
PKGS_LIBS!=	pkg-config --libs ${PKGS}
CPPFLAGS+=	${PKGS_CFLAGS}
LDADD+=		${PKGS_LIBS}
.endif
