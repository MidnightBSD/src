#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: release/7.0.0/bin/ls/Makefile 129719 2004-05-25 14:53:47Z pjd $

PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAP}
LDADD+=	-ltermcap
.endif

.include <bsd.prog.mk>
