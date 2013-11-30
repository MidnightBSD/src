# $Id: Makefile,v 1.1.1.2 2006-02-25 02:38:24 laffer1 Exp $
# $FreeBSD: src/usr.sbin/bluetooth/bthidcontrol/Makefile,v 1.1 2004/04/10 00:18:00 emax Exp $

.PATH:	${.CURDIR}/../bthidd

PROG=		bthidcontrol
MAN=		bthidcontrol.8
SRCS=		bthidcontrol.c hid.c lexer.l  parser.y sdp.c
WARNS?=		1
CFLAGS+=	-DBTHIDCONTROL=1 -I${.CURDIR}/../bthidd

DPADD=		${LIBBLUETOOTH} ${LIBSDP} ${LIBUSBHID}
LDADD=		-lbluetooth -lsdp -lusbhid

.include <bsd.prog.mk>
