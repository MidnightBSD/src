# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: stable/10/bin/date/Makefile 262951 2014-03-09 17:04:31Z jmmv $

.include <bsd.own.mk>

PROG=	date
SRCS=	date.c netdate.c vary.c

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
