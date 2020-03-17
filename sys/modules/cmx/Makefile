# $FreeBSD: stable/11/sys/modules/cmx/Makefile 319182 2017-05-30 04:11:12Z ngie $
#
.PATH:	${SRCTOP}/sys/dev/cmx

KMOD=	cmx
SRCS=	cmx.c cmxvar.h cmxreg.h \
	cmx_pccard.c \
	pccarddevs.h device_if.h bus_if.h card_if.h

.include <bsd.kmod.mk>
