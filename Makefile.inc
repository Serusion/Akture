# Settings shared by all programs.

# Where Akture is installed and where it looks for its files.  The
# defaults are those of the base system; a package overrides them,
# e.g. make AKTURE_PREFIX=/usr/pkg AKTURE_ETCDIR=/usr/pkg/etc/akture.
# AKTURE_XSERVER, AKTURE_FONT_UI and AKTURE_FONT_CLOCK, if set, replace
# the defaults of x_server, font_ui and font_clock.
AKTURE_PREFIX?=		/usr
AKTURE_ETCDIR?=		/etc/akture
AKTURE_SHAREDIR?=	${AKTURE_PREFIX}/share/akture
AKTURE_DBDIR?=		/var/db/akture
AKTURE_RUNDIR?=		/var/run/akture

WARNS?=		5
CFLAGS+=	-std=c11
CPPFLAGS+=	-D_NETBSD_SOURCE -I${.CURDIR}/../common
CPPFLAGS+=	-D_PATH_AKTURE_ETC=\"${AKTURE_ETCDIR}\"
CPPFLAGS+=	-D_PATH_AKTURE_SHARE=\"${AKTURE_SHAREDIR}\"
CPPFLAGS+=	-D_PATH_AKTURE_DB=\"${AKTURE_DBDIR}\"
CPPFLAGS+=	-D_PATH_AKTURE_RUN=\"${AKTURE_RUNDIR}\"
CPPFLAGS+=	-D_PATH_AKTURE_GREETER=\"${AKTURE_PREFIX}/libexec/akture-greeter\"
.if defined(AKTURE_XSERVER)
CPPFLAGS+=	-D_PATH_AKTURE_XSERVER=\"${AKTURE_XSERVER}\"
.endif
.if defined(AKTURE_FONT_UI)
CPPFLAGS+=	-D_PATH_AKTURE_FONT_UI=\"${AKTURE_FONT_UI}\"
.endif
.if defined(AKTURE_FONT_CLOCK)
CPPFLAGS+=	-D_PATH_AKTURE_FONT_CLOCK=\"${AKTURE_FONT_CLOCK}\"
.endif
.PATH:		${.CURDIR}/../common

# Flags of the pkg-config(1) packages listed in PKGS by a program.
.if defined(PKGS) && !empty(PKGS)
PKGS_CFLAGS!=	pkg-config --cflags ${PKGS}
PKGS_LIBS!=	pkg-config --libs ${PKGS}
CPPFLAGS+=	${PKGS_CFLAGS}
LDADD+=		${PKGS_LIBS}
.endif
