# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/stat/Makefile 321085 2017-07-17 21:01:07Z ngie $

.include <bsd.own.mk>

PROG=	stat

LINKS=	${BINDIR}/stat ${BINDIR}/readlink
MLINKS=	stat.1 readlink.1

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
