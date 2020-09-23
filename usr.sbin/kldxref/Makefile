# $FreeBSD: stable/11/usr.sbin/kldxref/Makefile 275904 2014-12-18 16:57:19Z imp $

PROG=	kldxref
MAN=	kldxref.8
SRCS=	kldxref.c ef.c ef_obj.c

WARNS?=	2

.if exists(ef_${MACHINE_CPUARCH}.c) && ${MACHINE_ARCH} != "powerpc64"
SRCS+=	ef_${MACHINE_CPUARCH}.c
.else
SRCS+=	ef_nop.c
.endif

.include <bsd.prog.mk>
