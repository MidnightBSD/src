# $Id: Makefile,v 1.1.1.2 2006-02-25 02:38:11 laffer1 Exp $
# $FreeBSD: src/usr.bin/bluetooth/btsockstat/Makefile,v 1.4 2005/01/28 16:08:07 ru Exp $

PROG=		btsockstat
WARNS?=		2
BINGRP=		kmem
BINMODE=	2555

DPADD=		${LIBBLUETOOTH} ${LIBKVM}
LDADD=		-lbluetooth -lkvm

.include <bsd.prog.mk>
