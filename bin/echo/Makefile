#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: stable/11/bin/echo/Makefile 320711 2017-07-06 05:28:32Z ngie $

.include <src.opts.mk>

PACKAGE=runtime
PROG=	echo

.if ${MK_TESTS} != "no"
SUBDIR+= 	tests
.endif

.include <bsd.prog.mk>
