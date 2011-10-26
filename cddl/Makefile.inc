# $MidnightBSD: src/cddl/Makefile.inc,v 1.1 2008/10/12 22:26:07 laffer1 Exp $

OPENSOLARIS_USR_DISTDIR= ${.CURDIR}/../../../cddl/contrib/opensolaris
OPENSOLARIS_SYS_DISTDIR= ${.CURDIR}/../../../sys/cddl/contrib/opensolaris

IGNORE_PRAGMA=        YES

CFLAGS+=      -DNEED_SOLARIS_BOOLEAN

CSTD?=                gnu89
