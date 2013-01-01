# $Id: Makefile,v 1.2 2013-01-01 17:41:47 laffer1 Exp $
# $MidnightBSD$

.PATH:	${.CURDIR}/../bthidd

PROG=		bthidcontrol
MAN=		bthidcontrol.8
SRCS=		bthidcontrol.c hid.c lexer.l  parser.y sdp.c
WARNS?=		1
CFLAGS+=	-DBTHIDCONTROL=1 -I${.CURDIR}/../bthidd

DPADD=		${LIBBLUETOOTH} ${LIBSDP} ${LIBUSBHID}
LDADD=		-lbluetooth -lsdp -lusbhid

.include <bsd.prog.mk>
