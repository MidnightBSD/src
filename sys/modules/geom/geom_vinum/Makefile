# $FreeBSD: release/10.0.0/sys/modules/geom/geom_vinum/Makefile 190507 2009-03-28 17:20:08Z lulf $

.PATH: ${.CURDIR}/../../../geom/vinum

KMOD=	geom_vinum
SRCS=	geom_vinum.c geom_vinum_create.c geom_vinum_drive.c geom_vinum_plex.c \
	geom_vinum_volume.c geom_vinum_subr.c geom_vinum_raid5.c \
	geom_vinum_share.c geom_vinum_list.c geom_vinum_rm.c \
	geom_vinum_init.c geom_vinum_state.c geom_vinum_rename.c \
	geom_vinum_move.c geom_vinum_events.c

.include <bsd.kmod.mk>
