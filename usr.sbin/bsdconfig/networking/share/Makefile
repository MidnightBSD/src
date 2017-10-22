# $FreeBSD: release/10.0.0/usr.sbin/bsdconfig/networking/share/Makefile 252746 2013-07-05 01:49:20Z dteske $

NO_OBJ=

FILESDIR=	${SHAREDIR}/bsdconfig/networking
FILES=		common.subr device.subr hostname.subr ipaddr.subr media.subr \
		netmask.subr resolv.subr routing.subr services.subr

beforeinstall:
	mkdir -p ${DESTDIR}${FILESDIR}

.include <bsd.prog.mk>
