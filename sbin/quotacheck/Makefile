# $FreeBSD: release/10.0.0/sbin/quotacheck/Makefile 207736 2010-05-07 00:41:12Z mckusick $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c utilities.c
WARNS?=	2
MAN=	quotacheck.8
DPADD=  ${LIBUTIL}
LDADD=  -lutil

.PATH:	${.CURDIR}/../fsck ${.CURDIR}/../fsck_ffs

.include <bsd.prog.mk>
