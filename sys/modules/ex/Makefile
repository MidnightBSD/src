# $FreeBSD: stable/11/sys/modules/ex/Makefile 319182 2017-05-30 04:11:12Z ngie $
#

.PATH:	${SRCTOP}/sys/dev/ex

KMOD=	if_ex
SRCS=	if_ex.c \
	if_ex_isa.c isa_if.h \
	if_ex_pccard.c card_if.h \
	bus_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
