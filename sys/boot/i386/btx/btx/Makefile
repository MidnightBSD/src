# $FreeBSD: release/10.0.0/sys/boot/i386/btx/btx/Makefile 235154 2012-05-09 08:04:29Z avg $

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
