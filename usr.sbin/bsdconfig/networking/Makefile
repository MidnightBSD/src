# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/networking/Makefile 240684 2012-09-18 22:28:42Z dteske $

NO_OBJ=

SUBDIR= include share

FILESDIR=	${LIBEXECDIR}/bsdconfig/120.networking
FILES=		INDEX USAGE

SCRIPTSDIR=	${FILESDIR}
SCRIPTS=	defaultrouter devices hostname nameservers networking

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
