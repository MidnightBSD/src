# $FreeBSD: stable/11/sbin/quotacheck/Makefile 319185 2017-05-30 04:17:22Z ngie $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PACKAGE=quotacheck
PROG=	quotacheck
SRCS=	quotacheck.c preen.c fsutil.c utilities.c
WARNS?=	2
MAN=	quotacheck.8
LIBADD=	util

.PATH:	${.CURDIR:H}/fsck ${.CURDIR:H}/fsck_ffs

.include <bsd.prog.mk>
