# $MidnightBSD: src/usr.bin/pcc/cpp/Makefile,v 1.5 2008/05/15 05:55:30 laffer1 Exp $

.include <bsd.own.mk>

PROG=	pcpp
TARGOS=	midnightbsd
BINDIR=	${libexecdir}
NO_MAN=
#MAN=	cpp.1
#MANDIR=	/usr/local/man/man

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I. -I${.CURDIR}
SRCS=	cpy.y scanner.l cpp.c

CLEANFILES+=	y.tab.c y.tab.h 

#scanner.l expects y.tab.h
cpy.c: cpy.y
	${YACC} ${YFLAGS} -o ${.TARGET} ${.ALLSRC}
	mv cpy.h y.tab.h

.include <bsd.prog.mk>
