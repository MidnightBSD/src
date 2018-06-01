# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/i386/pmbr/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	pmbr
STRIP=
BINMODE=${NOBINMODE}
MAN=
SRCS=	${PROG}.s

ORG=	0x600

AFLAGS+=--defsym FLAGS=${BOOT_MBR_FLAGS}
LDFLAGS=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>
