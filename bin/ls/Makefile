#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: release/10.0.0/bin/ls/Makefile 235655 2012-05-19 18:05:00Z marcel $

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

.include <bsd.prog.mk>
