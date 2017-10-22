# $FreeBSD: release/7.0.0/sys/modules/geom/geom_vinum/Makefile 152773 2005-11-24 15:11:41Z le $

.PATH: ${.CURDIR}/../../../geom/vinum

KMOD=	geom_vinum
SRCS=	geom_vinum.c geom_vinum_drive.c geom_vinum_plex.c \
	geom_vinum_volume.c geom_vinum_subr.c geom_vinum_raid5.c \
	geom_vinum_share.c geom_vinum_list.c geom_vinum_rm.c \
	geom_vinum_init.c geom_vinum_state.c geom_vinum_rename.c \
	geom_vinum_move.c

.include <bsd.kmod.mk>
