# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/du/Makefile 321133 2017-07-18 16:49:24Z ngie $

.include <bsd.own.mk>

PROG=	du
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
