# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/pc98/cdboot/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
MAN=
SRCS=	${PROG}.S

CFLAGS+=-I${.CURDIR}/../../i386/common

ORG=	0x0000

LDFLAGS=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.cdboot.S=	${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
