# $FreeBSD: stable/11/sys/modules/spi/spibus/Makefile 332113 2018-04-06 16:48:07Z ian $

.PATH:	${SRCTOP}/sys/dev/spibus

KMOD=	spibus
SRCS=	spibus.c

.if !empty(OPT_FDT)
SRCS+=	ofw_spibus.c ofw_bus_if.h
.endif

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	spibus_if.c \
	spibus_if.h \

.include <bsd.kmod.mk>
