#
# $FreeBSD: stable/11/sys/modules/gpio/gpiokeys/Makefile 319182 2017-05-30 04:11:12Z ngie $
#

.PATH:	${SRCTOP}/sys/dev/gpio/

KMOD=	gpiokeys
SRCS=	gpiokeys.c gpiokeys_codes.c
SRCS+=	bus_if.h device_if.h gpio_if.h ofw_bus_if.h
SRCS+=	opt_platform.h opt_kbd.h

CFLAGS+=  -I. -I${SRCTOP}/sys/dev/gpio/

.include <bsd.kmod.mk>
