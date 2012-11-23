# $Id: Makefile,v 1.2 2012-11-23 01:13:49 laffer1 Exp $
# $MidnightBSD$

PROG=		rfcomm_sppd
SRCS=		rfcomm_sppd.c rfcomm_sdp.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
