# $MidnightBSD: src/sys/boot/i386/btx/btx/Makefile,v 1.2 2011/10/16 21:11:08 laffer1 Exp $
# $FreeBSD: src/sys/boot/i386/btx/btx/Makefile,v 1.19.10.1.2.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	btx
INTERNALPROG=
NO_MAN=
SRCS=	btx.S

.if defined(BOOT_BTX_NOHANG)
BOOT_BTX_FLAGS=0x1
.else
BOOT_BTX_FLAGS=0x0
.endif

CFLAGS+=-DBTX_FLAGS=${BOOT_BTX_FLAGS}
CFLAGS+=-I${.CURDIR}/../../common

.if defined(BTX_SERIAL)
BOOT_COMCONSOLE_PORT?= 0x3f8
BOOT_COMCONSOLE_SPEED?= 9600
B2SIOFMT?=	0x3

CFLAGS+=-DBTX_SERIAL -DSIOPRT=${BOOT_COMCONSOLE_PORT} \
	-DSIOFMT=${B2SIOFMT} -DSIOSPD=${BOOT_COMCONSOLE_SPEED}
.endif

ORG=	0x9000

LDFLAGS=-e start -Ttext ${ORG} -Wl,-N,-S,--oformat,binary

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.btx.S=		${CLANG_NO_IAS}
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}
