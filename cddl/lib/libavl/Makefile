# $MidnightBSD: src/cddl/lib/libavl/Makefile,v 1.2 2008/10/15 19:10:19 laffer1 Exp $
# $FreeBSD: src/cddl/lib/libavl/Makefile,v 1.2 2007/04/16 21:20:24 ru Exp $

.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/avl

LIB=	avl
SRCS=	avl.c
WARNS?=	0
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common

.include <bsd.lib.mk>
