# $MidnightBSD: src/usr.bin/pcc/cpp/Makefile,v 1.1 2008/05/14 04:25:57 laffer1 Exp $

PROG=	cpp
PREFIX=	/usr/local
BINDIR=	${PREFIX}/libexec
MANDIR=	${PREFIX}/man/man
TARGOS=	midnightbsd

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}/libexec\"
CFLAGS+=	-I. -I${.CURDIR}
YFLAGS= -d
SRCS=	cpy.y scanner.l cpp.c

.include <bsd.prog.mk>
