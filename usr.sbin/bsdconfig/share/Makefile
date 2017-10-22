# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/share/Makefile 256361 2013-10-11 23:12:05Z dteske $

NO_OBJ=

SUBDIR=	media packages

FILESDIR=	${SHAREDIR}/bsdconfig
FILES=		common.subr device.subr dialog.subr keymap.subr \
		mustberoot.subr script.subr strings.subr struct.subr \
		sysrc.subr variable.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
