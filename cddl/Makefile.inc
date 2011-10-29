# $MidnightBSD: src/cddl/Makefile.inc,v 1.2 2011/10/26 14:00:17 laffer1 Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=	YES

CFLAGS+=	-DNEED_SOLARIS_BOOLEAN
CFLAGS+=	-D_SOLARIS_C_SOURCE

CSTD?=		gnu89
