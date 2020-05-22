# $FreeBSD: stable/11/usr.bin/ident/Makefile 285890 2015-07-26 11:21:36Z bapt $

.include <src.opts.mk>

PROG=	ident

LIBADD=	sbuf

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
