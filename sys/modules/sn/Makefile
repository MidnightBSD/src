# $FreeBSD: stable/11/sys/modules/sn/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/sn

KMOD=	if_sn
SRCS=	if_sn.c if_sn_isa.c if_sn_pccard.c device_if.h bus_if.h isa_if.h \
	card_if.h pccarddevs.h

.include <bsd.kmod.mk>
