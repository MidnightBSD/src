# $FreeBSD: stable/11/sys/modules/i2c/rtc8583/Makefile 330412 2018-03-04 21:06:21Z ian $

.PATH:	${SRCTOP}/sys/dev/iicbus
KMOD	= rtc8583
SRCS	= rtc8583.c 

# Generated files...
SRCS+=	\
	bus_if.h \
	clock_if.h \
	device_if.h \
	iicbus_if.h \
	opt_platform.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
