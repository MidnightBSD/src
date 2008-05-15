# $MidnightBSD: src/usr.bin/pcc/cpp/Makefile,v 1.2 2008/05/15 04:53:33 laffer1 Exp $

.include <bsd.own.mk>

PROG=	cpp
TARGOS=	midnightbsd
BINDIR=	${libexecdir}

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
