# $FreeBSD: stable/11/sys/modules/spi/mx25l/Makefile 332113 2018-04-06 16:48:07Z ian $

.PATH:	${SRCTOP}/sys/dev/flash

KMOD=	mx25l
SRCS=	mx25l.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	spibus_if.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
