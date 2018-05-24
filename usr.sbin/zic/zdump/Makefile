# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/zic/zdump/Makefile 291794 2015-12-04 18:21:05Z bdrewery $

.PATH: ${.CURDIR}/../../../contrib/tzcode/zic

PROG=	zdump
MAN=	zdump.8
SRCS=	zdump.c ialloc.c scheck.c

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"${SHAREDIR}/zoneinfo\" -Demkdir=mkdir
CFLAGS+= -I${.CURDIR}/.. -I${.CURDIR}/../../../contrib/tzcode/stdtime

WARNS?=	2

.include <bsd.prog.mk>
