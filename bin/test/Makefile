#        @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: stable/11/bin/test/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PACKAGE=runtime
PROG=	test
LINKS=	${BINDIR}/test ${BINDIR}/[
MLINKS=	test.1 [.1

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
