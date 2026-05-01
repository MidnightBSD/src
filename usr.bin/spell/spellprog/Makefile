#	$NetBSD: Makefile,v 1.3 2006/08/26 18:17:43 christos Exp $

.include <src.opts.mk>

PROG=	spellprog
MAN=	
SRCS=	spellprog.c look.c
BINDIR=	/usr/libexec

.if ${MK_TESTS} != "no"
HAS_TESTS=
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
