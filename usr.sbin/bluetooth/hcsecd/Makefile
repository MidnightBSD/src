# $Id: Makefile,v 1.1.1.1 2006-02-25 02:29:36 laffer1 Exp $
# $FreeBSD: src/usr.sbin/bluetooth/hcsecd/Makefile,v 1.4 2004/09/14 20:04:33 emax Exp $

PROG=		hcsecd
MAN=		hcsecd.8 hcsecd.conf.5
SRCS=		hcsecd.c lexer.l parser.y
WARNS?=		2
CFLAGS+=	-I${.CURDIR}

DPADD=		${LIBBLUETOOTH}
LDADD=		-lbluetooth

.include <bsd.prog.mk>
