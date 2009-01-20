# $MidnightBSD: src/usr.bin/pcc/cpp/Makefile,v 1.6 2008/06/25 19:49:04 laffer1 Exp $

.include <bsd.own.mk>

PROG=	pcpp
TARGOS=	midnightbsd
BINDIR=	${libexecdir}
MAN=	pcpp.1

CFLAGS+=	-DCPP_DEBUG -Wall -Wmissing-prototypes -Wstrict-prototypes -Werror
CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I. -I${.CURDIR} -I${.CURDIR}/.. -I${.CURDIR}/../mip/
SRCS=	cpy.y scanner.l cpp.c

CLEANFILES+=	y.tab.c y.tab.h 

#scanner.l expects y.tab.h
cpy.c: cpy.y
	${YACC} ${YFLAGS} -o ${.TARGET} ${.ALLSRC}
	mv cpy.h y.tab.h

.include <bsd.prog.mk>
