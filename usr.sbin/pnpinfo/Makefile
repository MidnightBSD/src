# $FreeBSD: release/7.0.0/usr.sbin/pnpinfo/Makefile 161526 2006-08-22 08:03:01Z ru $

.PATH: ${.CURDIR}/../../contrib/pnpinfo

PROG=	pnpinfo
MAN=	pnpinfo.8

CFLAGS+= -I${.CURDIR}/../../sys

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
