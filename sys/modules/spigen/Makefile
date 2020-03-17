# $FreeBSD: stable/11/sys/modules/spigen/Makefile 350066 2019-07-16 19:40:23Z eugen $

.PATH:	${SRCTOP}/sys/dev/spibus
KMOD=	spigen
SRCS=	spigen.c

# Generated files...
SRCS+=	\
	bus_if.h \
	device_if.h \
	opt_platform.h \
	opt_spi.h \
	spibus_if.h \

.if !empty(OPT_FDT)
SRCS+=	ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
