# $FreeBSD: stable/11/stand/i386/cdboot/Makefile 329183 2018-02-12 20:51:28Z kevans $

.include <bsd.init.mk>

PROG=	cdboot
STRIP=
BINMODE=${NOBINMODE}
SRCS=	${PROG}.S

CFLAGS+=-I${BOOTSRC}/i386/common

ORG=	0x7c00

LDFLAGS+=${LDFLAGS_BIN}

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.cdboot.S=	${CLANG_NO_IAS}
