# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/share/media/Makefile 252112 2013-06-23 10:48:26Z dteske $

NO_OBJ=

FILESDIR=	${SHAREDIR}/bsdconfig/media
FILES=		any.subr cdrom.subr common.subr directory.subr dos.subr \
		floppy.subr ftp.subr http.subr httpproxy.subr network.subr \
		nfs.subr options.subr tcpip.subr ufs.subr usb.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
