SUBDIR=		pam.d rc.d sessions

FILES=		akture.conf
FILESDIR=	/etc/akture
FILESMODE=	644

# Directories the programs expect.  /var/db/akture/resolutions is
# written by the greeter, everything else only by root.
afterinstall:
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/etc/akture/hooks.d
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/var/db/akture
	${INSTALL} -d -o root -g wheel -m 755 \
	    ${DESTDIR}/usr/share/akture/backgrounds
	${INSTALL} -d -o root -g wheel -m 755 ${DESTDIR}/usr/share/akture/fonts
	test -f ${DESTDIR}/var/db/akture/resolutions || \
	    ${INSTALL} -o _akture -g wheel -m 644 /dev/null \
	    ${DESTDIR}/var/db/akture/resolutions

.include <bsd.files.mk>
.include <bsd.subdir.mk>
