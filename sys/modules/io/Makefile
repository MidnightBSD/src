# $FreeBSD: stable/11/sys/modules/io/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/io
.PATH: ${SRCTOP}/sys/${MACHINE_CPUARCH}/${MACHINE_CPUARCH}

KMOD=	io
SRCS=	iodev.c io.c
SRCS+=	bus_if.h device_if.h

.include <bsd.kmod.mk>
