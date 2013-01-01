# $Id: Makefile,v 1.2 2013-01-01 17:41:47 laffer1 Exp $
# $MidnightBSD$

PROG=		l2control
MAN=		l2control.8
SRCS=		l2cap.c l2control.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
