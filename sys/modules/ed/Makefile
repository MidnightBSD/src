# $FreeBSD: stable/11/sys/modules/ed/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/ed

KMOD=	if_ed
SRCS=	if_ed.c
SRCS+=	if_ed_novell.c if_ed_wd80x3.c if_ed_rtl80x9.c isa_if.h
.if ${MACHINE} == "pc98"
SRCS+=	if_ed_cbus.c
.else
SRCS+=	if_ed_isa.c
SRCS.ED_HPP=if_ed_hpp.c
SRCS.ED_SIC=if_ed_sic.c
SRCS.ED_3C503=if_ed_3c503.c
.endif
SRCS+=	if_ed_pccard.c pccarddevs.h card_if.h
SRCS.DEV_PCI=if_ed_pci.c pci_if.h

SRCS+=	opt_ed.h bus_if.h device_if.h miibus_if.h

.include <bsd.kmod.mk>
