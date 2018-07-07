# $MidnightBSD$
# $Id: Makefile,v 1.7 2003/09/07 18:15:55 max Exp $
# $FreeBSD: stable/10/usr.bin/bluetooth/rfcomm_sppd/Makefile 274182 2014-11-06 16:32:00Z emax $

PROG=		rfcomm_sppd
SRCS=		rfcomm_sppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp -lutil

.include <bsd.prog.mk>
