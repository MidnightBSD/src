# $FreeBSD: release/10.0.0/usr.sbin/diskinfo/Makefile 201390 2010-01-02 11:07:44Z ed $

PROG=	diskinfo
MAN=	diskinfo.8

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad4 md50
	./${PROG} -v /dev/ad4 md50
	./${PROG} -t /dev/ad4
