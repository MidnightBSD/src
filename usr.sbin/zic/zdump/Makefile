# $MidnightBSD: src/usr.sbin/zic/zdump/Makefile,v 1.3 2012/12/02 04:22:23 laffer1 Exp $

.PATH: ${.CURDIR}/../../../contrib/tzcode/zic

PROG=	zdump
MAN=	zdump.8
SRCS=	zdump.c ialloc.c scheck.c

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../contrib/tzcode/stdtime

WARNS?=	2

.include <bsd.prog.mk>
