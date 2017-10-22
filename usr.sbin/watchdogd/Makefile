# $FreeBSD: stable/9/usr.sbin/watchdogd/Makefile 243820 2012-12-03 18:30:12Z delphij $

PROG=	watchdogd
LINKS=	${BINDIR}/watchdogd ${BINDIR}/watchdog
MAN=	watchdogd.8 watchdog.8

LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} -t 1.0
