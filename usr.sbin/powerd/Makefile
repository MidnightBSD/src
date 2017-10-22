# $FreeBSD: release/7.0.0/usr.sbin/powerd/Makefile 153999 2006-01-03 21:32:02Z des $

PROG=	powerd
MAN=	powerd.8
WARNS?=	6

DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MACHINE_ARCH} == "i386"
CFLAGS+=-DUSE_APM
.endif

.include <bsd.prog.mk>
