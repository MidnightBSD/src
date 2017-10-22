# $FreeBSD: release/10.0.0/sys/modules/mem/Makefile 221869 2011-05-14 01:53:38Z attilio $

.PATH: ${.CURDIR}/../../dev/mem
.PATH: ${.CURDIR}/../../${MACHINE}/${MACHINE}
.PATH: ${.CURDIR}/../../${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

KMOD=	mem
SRCS=	mem.c memdev.c memutil.c
.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	i686_mem.c k6_mem.c
.endif
.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	amd64_mem.c
.endif
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
