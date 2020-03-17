
# $FreeBSD: stable/11/sys/modules/puc/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:	${SRCTOP}/sys/dev/puc

KMOD=	puc
SRCS=	puc.c puc_cfg.c puc_pci.c puc_pccard.c pucdata.c
SRCS+=	bus_if.h device_if.h serdev_if.c serdev_if.h \
	card_if.h pci_if.h

.include <bsd.kmod.mk>
