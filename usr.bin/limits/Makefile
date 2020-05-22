# $FreeBSD: stable/11/usr.bin/limits/Makefile 290567 2015-11-09 01:05:31Z ngie $

.include <src.opts.mk>

PROG=	limits
LIBADD=	util

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
