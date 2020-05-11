# $Id: Makefile,v 1.8 2003/08/14 20:06:20 max Exp $
# $FreeBSD: stable/11/usr.sbin/bluetooth/hcsecd/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=		hcsecd
MAN=		hcsecd.8 hcsecd.conf.5
SRCS=		hcsecd.c lexer.l parser.y
WARNS?=		2
CFLAGS+=	-I${.CURDIR}

LIBADD=		bluetooth

.include <bsd.prog.mk>
