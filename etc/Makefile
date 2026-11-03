SUBDIR=		pam.d rc.d sessions

FILES=		akture.conf
FILESDIR=	${AKTURE_ETCDIR}
FILESMODE=	644

# Directories the programs expect.  The resolutions file is written by
# the greeter, everything else only by root.
afterinstall:
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}${AKTURE_ETCDIR}/hooks.d
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}${AKTURE_DBDIR}
	${INSTALL} -d -o root -g wheel -m 755 \
	    ${DESTDIR}${AKTURE_SHAREDIR}/backgrounds
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}${AKTURE_SHAREDIR}/fonts
	test -f ${DESTDIR}${AKTURE_DBDIR}/resolutions || \
	    ${INSTALL} -o _akture -g wheel -m 644 /dev/null \
	    ${DESTDIR}${AKTURE_DBDIR}/resolutions

.include <bsd.files.mk>
.include <bsd.subdir.mk>
