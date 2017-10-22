# $FreeBSD: release/7.0.0/lib/libc_r/sys/Makefile.inc 50476 1999-08-28 00:22:10Z peter $

.PATH:	 ${.CURDIR}/sys ${.CURDIR}/arch/${MACHINE_ARCH}

SRCS+=	uthread_error.c _atomic_lock.S

