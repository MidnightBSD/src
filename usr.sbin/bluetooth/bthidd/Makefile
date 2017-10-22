# $Id: Makefile,v 1.6 2006/09/07 21:36:55 max Exp $
# $FreeBSD: stable/9/usr.sbin/bluetooth/bthidd/Makefile 204596 2010-03-02 18:44:08Z uqs $

PROG=		bthidd
MAN=		bthidd.8
#		bthidd.conf.5
SRCS=		bthidd.c client.c hid.c kbd.c lexer.l parser.y server.c \
		session.c

CFLAGS+=	-I${.CURDIR}
DEBUG_FLAGS=	-g

DPADD=          ${LIBBLUETOOTH} ${LIBUSBHID}
LDADD=		-lbluetooth -lusbhid

.include <bsd.prog.mk>
