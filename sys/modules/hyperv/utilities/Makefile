# $FreeBSD: stable/11/sys/modules/hyperv/utilities/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/hyperv/utilities

KMOD=	hv_utils
SRCS=	vmbus_ic.c
SRCS+=	hv_kvp.c
SRCS+=	hv_snapshot.c
SRCS+=	vmbus_heartbeat.c
SRCS+=	vmbus_shutdown.c
SRCS+=	vmbus_timesync.c
SRCS+=	bus_if.h device_if.h vmbus_if.h

CFLAGS+= -I${SRCTOP}/sys/dev/hyperv/include \
	 -I${SRCTOP}/sys/dev/hyperv/vmbus

.include <bsd.kmod.mk>
