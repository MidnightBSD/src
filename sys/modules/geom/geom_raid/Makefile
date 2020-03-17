# $FreeBSD: stable/11/sys/modules/geom/geom_raid/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/geom/raid

KMOD=	geom_raid
SRCS=	g_raid.c
SRCS+=	g_raid_ctl.c
SRCS+=	bus_if.h device_if.h
SRCS+=	g_raid_md_if.h g_raid_md_if.c
SRCS+=	g_raid_tr_if.h g_raid_tr_if.c

SRCS+=	md_ddf.c md_intel.c md_jmicron.c md_nvidia.c md_promise.c md_sii.c

SRCS+=	tr_concat.c tr_raid0.c tr_raid1.c tr_raid1e.c tr_raid5.c

.include <bsd.kmod.mk>
