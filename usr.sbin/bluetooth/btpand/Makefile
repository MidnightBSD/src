# $NetBSD: Makefile,v 1.2 2008/08/18 08:25:32 plunky Exp $
# $FreeBSD: stable/11/usr.sbin/bluetooth/btpand/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	btpand
MAN=	btpand.8
SRCS=	btpand.c bnep.c channel.c client.c event.c packet.c server.c sdp.c tap.c

WARNS?=	3

LIBADD=	bluetooth sdp util

.include <bsd.prog.mk>
