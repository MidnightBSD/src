#	$OpenBSD: Makefile,v 1.1 2004/09/28 15:10:51 millert Exp $
#       $MidnightBSD$

SUDO_DIR=       ${.CURDIR}/../../../contrib/sudo
.PATH:		${SUDO_DIR}

LIB=		noexec
NO_MAN=		yes
SRCS=		sudo_noexec.c
CFLAGS+=	-I${SUDO_DIR}
LIBDIR=		/usr/libexec

SHLIB_MAJOR=	3

DEBUGLIBS=	no
NO_PROFILE= 	yes

.include <bsd.lib.mk>
