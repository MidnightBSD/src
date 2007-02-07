#	$OpenBSD: Makefile,v 1.1 2004/01/15 21:32:03 millert Exp $
#       $MidnightBSD: src/usr.bin/sudo/visudo/Makefile,v 1.1 2007/02/04 17:33:53 archite Exp $

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
.PATH:		${SUDO_DIR}

PROG=		visudo
INSTALLFLAGS=   -S
BINDIR=		/usr/bin
CFLAGS+=	-I${SUDO_DIR} -I${.CURDIR}/../lib
LDADD=         ${LIBSUDO}
DPADD=         ${LIBSUDO}
MAN=		visudo.8

.include <bsd.prog.mk>

