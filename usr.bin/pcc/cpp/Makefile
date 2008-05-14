# $MidnightBSD$

PROG=	cpp
PREFIX=	/usr/local
BINDIR=	${PREFIX}/libexec
MANDIR=	${PREFIX}/man
TARGOS=	midnightbsd

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CPPFLAGS+=	-I. -I${.CURDIR}

SRCS=	cpy.y scanner.l cpp.c

.include <bsd.prog.mk>
