#
# $FreeBSD: stable/11/sys/modules/tests/framework/Makefile 319182 2017-05-30 04:11:12Z ngie $
#

.PATH: ${SRCTOP}/sys/tests/framework

KMOD=	kern_testfrwk
SRCS=	kern_testfrwk.c
SRCS+=	bus_if.h device_if.h

#
# Enable full debugging
#
#CFLAGS += -g

.include <bsd.kmod.mk>
