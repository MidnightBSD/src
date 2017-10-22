# $FreeBSD: release/7.0.0/usr.sbin/diskinfo/Makefile 129679 2004-05-24 22:52:32Z pjd $

PROG=	diskinfo
MAN=	diskinfo.8

WARNS?=	5

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>

test:	${PROG}
	./${PROG} /dev/ad4 md50
	./${PROG} -v /dev/ad4 md50
	./${PROG} -t /dev/ad4
