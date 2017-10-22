# $FreeBSD: release/10.0.0/sys/modules/cmx/Makefile 241828 2012-10-22 02:12:11Z eadler $
#
.PATH:	${.CURDIR}/../../dev/cmx

KMOD=	cmx
SRCS=	cmx.c cmxvar.h cmxreg.h \
	cmx_pccard.c \
	pccarddevs.h device_if.h bus_if.h card_if.h

.include <bsd.kmod.mk>
