# $FreeBSD: stable/11/sys/modules/i2c/iicbb/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:		${SRCTOP}/sys/dev/iicbus
.if ${MACHINE_CPUARCH} == "aarch64" || ${MACHINE_CPUARCH} == "arm" || \
    ${MACHINE_CPUARCH} == "mips" || ${MACHINE_CPUARCH} == "powerpc"
ofw_bus_if=	ofw_bus_if.h
.endif
KMOD		= iicbb
SRCS		= device_if.h bus_if.h iicbus_if.h \
		  iicbb_if.h iicbb_if.c iicbb.c ${ofw_bus_if} opt_platform.h

.include <bsd.kmod.mk>
