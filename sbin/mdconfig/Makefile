# $FreeBSD: stable/11/sbin/mdconfig/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PACKAGE=runtime
PROG=		mdconfig
MAN=		mdconfig.8

LIBADD=		util geom

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
