# $Id: Makefile,v 1.2 2013-01-01 17:41:47 laffer1 Exp $
# $MidnightBSD$

PROG=		sdpcontrol
MAN=		sdpcontrol.8
SRCS=		sdpcontrol.c search.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lsdp

.include <bsd.prog.mk>
