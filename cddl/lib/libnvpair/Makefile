# $MidnightBSD: src/cddl/lib/libnvpair/Makefile,v 1.2 2008/10/15 19:10:19 laffer1 Exp $
# $FreeBSD: src/cddl/lib/libnvpair/Makefile,v 1.2 2007/04/16 21:20:25 ru Exp $

.PATH: ${.CURDIR}/../../../cddl/contrib/opensolaris/lib/libnvpair
.PATH: ${.CURDIR}/../../../sys/cddl/contrib/opensolaris/common/nvpair

LIB=	nvpair

SRCS=	libnvpair.c \
	nvpair_alloc_system.c \
	nvpair_alloc_fixed.c \
	nvpair.c

WARNS?=	0
CFLAGS+= -I${.CURDIR}/../../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../../sys

.include <bsd.lib.mk>
