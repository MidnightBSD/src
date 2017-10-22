# $FreeBSD: release/7.0.0/usr.bin/ldd/Makefile 131291 2004-06-29 21:13:15Z dwmalone $

PROG=		ldd
SRCS=		ldd.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=		sods.c
.endif
WARNS?=		6

.include <bsd.prog.mk>
