# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/truncate/Makefile 275585 2014-12-07 22:30:54Z jilles $

.include <bsd.own.mk>

PROG=		truncate
DPADD=		${LIBUTIL}
LDADD=		-lutil

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
