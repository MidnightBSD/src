# $Id: Makefile,v 1.2 2004/02/13 21:44:41 max Exp $
# $FreeBSD: stable/11/usr.sbin/bluetooth/bthidcontrol/Makefile 319119 2017-05-29 10:15:41Z ngie $

.PATH:	${.CURDIR:H}/bthidd

PROG=		bthidcontrol
MAN=		bthidcontrol.8
SRCS=		bthidcontrol.c hid.c lexer.l  parser.y sdp.c
WARNS?=		1
CFLAGS+=	-DBTHIDCONTROL=1 -I${.CURDIR:H}/bthidd

LIBADD+=	bluetooth sdp usbhid

.include <bsd.prog.mk>
