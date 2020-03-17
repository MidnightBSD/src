# $FreeBSD: stable/11/sys/modules/syscons/beastie/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/syscons/logo

KMOD=	beastie_saver
SRCS=	beastie.c logo_saver.c

CFLAGS+=-DBEASTIE_LOGO

.include <bsd.kmod.mk>
