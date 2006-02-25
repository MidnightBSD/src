# $Id: Makefile,v 1.1.1.2 2006-02-25 02:38:25 laffer1 Exp $
# $FreeBSD: src/usr.sbin/bluetooth/rfcomm_pppd/Makefile,v 1.2 2003/10/12 22:04:24 emax Exp $

.PATH:	${.CURDIR}/../../../usr.bin/bluetooth/rfcomm_sppd

PROG=		rfcomm_pppd
MAN=		rfcomm_pppd.8
SRCS=		rfcomm_pppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
