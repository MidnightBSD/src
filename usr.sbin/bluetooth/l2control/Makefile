# $Id: Makefile,v 1.1.1.1 2006-02-25 02:29:36 laffer1 Exp $
# $FreeBSD: src/usr.sbin/bluetooth/l2control/Makefile,v 1.3 2003/10/12 22:04:23 emax Exp $

PROG=		l2control
MAN=		l2control.8
SRCS=		l2cap.c l2control.c
WARNS?=		2

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
