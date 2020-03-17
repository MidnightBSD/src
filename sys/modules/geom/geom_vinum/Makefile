# $FreeBSD: stable/11/sys/modules/geom/geom_vinum/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/geom/vinum

KMOD=	geom_vinum
SRCS=	geom_vinum.c geom_vinum_create.c geom_vinum_drive.c geom_vinum_plex.c \
	geom_vinum_volume.c geom_vinum_subr.c geom_vinum_raid5.c \
	geom_vinum_share.c geom_vinum_list.c geom_vinum_rm.c \
	geom_vinum_init.c geom_vinum_state.c geom_vinum_rename.c \
	geom_vinum_move.c geom_vinum_events.c

.include <bsd.kmod.mk>
