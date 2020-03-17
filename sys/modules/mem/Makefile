# $FreeBSD: stable/11/sys/modules/mem/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mem
.PATH: ${SRCTOP}/sys/${MACHINE}/${MACHINE}
.PATH: ${SRCTOP}/sys/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
.PATH: ${SRCTOP}/sys/x86/x86
.endif

KMOD=	mem
SRCS=	mem.c memdev.c memutil.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	x86_mem.c k6_mem.c
.endif
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	x86_mem.c
.endif
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
