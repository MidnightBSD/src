# $FreeBSD: stable/9/sys/modules/cmx/Makefile 242112 2012-10-26 00:44:32Z eadler $
#
.PATH:	${.CURDIR}/../../dev/cmx

KMOD=	cmx
SRCS=	cmx.c cmxvar.h cmxreg.h \
	cmx_pccard.c \
	pccarddevs.h device_if.h bus_if.h card_if.h

.include <bsd.kmod.mk>
