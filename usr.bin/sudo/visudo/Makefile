#	$OpenBSD: Makefile,v 1.1 2004/01/15 21:32:03 millert Exp $
#       $MidnightBSD$

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
.PATH:		${SUDO_DIR}

PROG=		visudo
INSTALLFLAGS=   -S
BINDIR=		/usr/bin
CFLAGS+=	-I${SUDO_DIR}
LDADD+=         -L${.OBJDIR}/../lib -lsudo
DPADD+=         -L${.OBJDIR}/../lib/libsudo.a
MAN=		visudo.8

.include <bsd.prog.mk>

