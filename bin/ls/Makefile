# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: stable/10/bin/ls/Makefile 289965 2015-10-25 23:03:04Z ngie $

.include <bsd.own.mk>

PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if !defined(RELEASE_CRUNCH) && \
	${MK_LS_COLORS} != no
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAP}
LDADD+=	-ltermcap
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
