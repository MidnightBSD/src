# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/limits/Makefile 290898 2015-11-16 00:58:31Z ngie $

.include <bsd.own.mk>

PROG=	limits
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
