#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/indent/Makefile 331147 2018-03-18 22:19:52Z eadler $

.include <src.opts.mk>

PROG=	indent
SRCS=	indent.c io.c lexi.c parse.c pr_comment.c args.c

NO_WMISSING_VARIABLE_DECLARATIONS=

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
