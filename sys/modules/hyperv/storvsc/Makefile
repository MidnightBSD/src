# $FreeBSD: stable/11/sys/modules/hyperv/storvsc/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/dev/hyperv/storvsc \
	${SRCTOP}/sys/dev/hyperv/vmbus \

KMOD=	hv_storvsc
SRCS =	hv_storvsc_drv_freebsd.c \
	hv_vstorage.h
SRCS+=	bus_if.h device_if.h opt_cam.h vmbus_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/include \
	 -I${SRCTOP}/sys/dev/hyperv/vmbus	\
	 -I${SRCTOP}/sys/dev/hyperv/storvsc

.include <bsd.kmod.mk>
