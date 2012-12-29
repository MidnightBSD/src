# $MidnightBSD: src/sys/boot/i386/pmbr/Makefile,v 1.1 2011/10/16 21:11:09 laffer1 Exp $
# $FreeBSD: src/sys/boot/i386/pmbr/Makefile,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
NO_MAN=
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>
