# $Id: Makefile,v 1.2 2012-11-23 01:13:49 laffer1 Exp $
# $MidnightBSD$

PROG=		btsockstat
WARNS?=		2
BINGRP=		kmem
BINMODE=	2555

DPADD=		${LIBBLUETOOTH} ${LIBKVM}
LDADD=		-lbluetooth -lkvm

.include <bsd.prog.mk>
