# $FreeBSD: release/10.0.0/usr.bin/ldd/Makefile 211725 2010-08-23 22:24:11Z imp $

PROG?=		ldd
SRCS=		ldd.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=		sods.c
.endif

.include <bsd.prog.mk>
