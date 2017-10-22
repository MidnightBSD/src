# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/timezone/share/Makefile 240684 2012-09-18 22:28:42Z dteske $

NO_OBJ=

FILESDIR=	${SHAREDIR}/bsdconfig/timezone
FILES=		continents.subr countries.subr iso3166.subr menus.subr \
		zones.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
