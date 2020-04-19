#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: stable/11/bin/ls/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
LIBADD=	xo util

.if !defined(RELEASE_CRUNCH) && \
	${MK_LS_COLORS} != no
CFLAGS+= -DCOLORLS
LIBADD+=	termcapw
.endif

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
