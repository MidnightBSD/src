# $FreeBSD: stable/11/lib/libc/x86/sys/Makefile.inc 313539 2017-02-10 07:38:39Z ngie $

.PATH:	${LIBC_SRCTOP}/x86/sys

SRCS+= \
	__vdso_gettc.c

.if ${MACHINE_CPUARCH} == "amd64" && ${MK_HYPERV} != "no"
CFLAGS+=	-DWANT_HYPERV
.endif
