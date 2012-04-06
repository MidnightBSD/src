# $MidnightBSD: src/cddl/Makefile.inc,v 1.3 2011/10/29 23:31:34 laffer1 Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN

CSTD?=		gnu89
