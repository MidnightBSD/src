# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/hyperv/utilities/Makefile 311254 2017-01-04 05:24:16Z sephe $

.PATH:	${.CURDIR}/../../../dev/hyperv/utilities

KMOD=	hv_utils
SRCS=	vmbus_ic.c
SRCS+=	hv_kvp.c
SRCS+=	hv_snapshot.c
SRCS+=	vmbus_heartbeat.c
SRCS+=	vmbus_shutdown.c
SRCS+=	vmbus_timesync.c
SRCS+=	bus_if.h device_if.h vmbus_if.h

CFLAGS+= -I${.CURDIR}/../../../dev/hyperv/include \
	 -I${.CURDIR}/../../../dev/hyperv/vmbus

.include <bsd.kmod.mk>
