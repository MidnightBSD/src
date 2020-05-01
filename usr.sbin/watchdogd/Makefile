# $FreeBSD: stable/11/usr.sbin/watchdogd/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8

LIBADD=	util

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
