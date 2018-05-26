# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/hyperv/storvsc/Makefile 307018 2016-10-11 06:35:29Z sephe $

.PATH:  ${.CURDIR}/../../../dev/hyperv/storvsc \
	${.CURDIR}/../../../dev/hyperv/vmbus \

KMOD=	hv_storvsc
SRCS =	hv_storvsc_drv_freebsd.c \
	hv_vstorage.h
SRCS+=	bus_if.h device_if.h opt_cam.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus	\
	 -I${.CURDIR}/../../../dev/hyperv/storvsc

.include <bsd.kmod.mk>
