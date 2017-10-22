# $FreeBSD: release/10.0.0/sys/modules/syscons/beastie/Makefile 206380 2010-04-07 18:42:21Z jkim $

.PATH:	${.CURDIR}/../../../dev/syscons/logo

KMOD=	beastie_saver
SRCS=	beastie.c logo_saver.c

CFLAGS+=-DBEASTIE_LOGO

.include <bsd.kmod.mk>
