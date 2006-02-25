# $Id: Makefile,v 1.1.1.1 2006-02-25 02:29:36 laffer1 Exp $
# $FreeBSD: src/usr.sbin/bluetooth/bthidd/Makefile,v 1.4 2004/12/21 08:47:31 ru Exp $

PROG=		bthidd
#MAN=		bthidd.8 bthidd.conf.5
NO_MAN=		1
SRCS=		bthidd.c client.c hid.c kbd.c lexer.l parser.y server.c \
		session.c

CFLAGS+=	-I${.CURDIR}
WARNS?=		2
DEBUG_FLAGS=	-g

DPADD=          ${LIBBLUETOOTH} ${LIBSDP}
LDADD=		-lbluetooth -lusbhid

.include <bsd.prog.mk>
