# $FreeBSD: stable/9/sys/boot/ofw/Makefile.inc 209920 2010-07-12 00:49:22Z nwhitehorn $

.if ${MACHINE_ARCH} == "powerpc64"
CFLAGS+=	-m32 -mcpu=powerpc
LDFLAGS+=	-m elf32ppc
.endif

.include "../Makefile.inc"
