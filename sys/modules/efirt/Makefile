# $FreeBSD: stable/11/sys/modules/efirt/Makefile 331025 2018-03-15 20:29:48Z kevans $

.PATH: ${SRCTOP}/sys/${MACHINE}/${MACHINE}
.PATH: ${SRCTOP}/sys/dev/efidev

KMOD=	efirt
SRCS=	efirt.c efirt_machdep.c efidev.c
SRCS+=	efirtc.c
SRCS+=  device_if.h bus_if.h clock_if.h

.include <bsd.kmod.mk>
