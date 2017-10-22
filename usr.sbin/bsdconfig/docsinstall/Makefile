# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/docsinstall/Makefile 238438 2012-07-14 03:16:57Z dteske $

NO_OBJ=

SUBDIR= include

FILESDIR=	${LIBEXECDIR}/bsdconfig/020.docsinstall
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	docsinstall

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
