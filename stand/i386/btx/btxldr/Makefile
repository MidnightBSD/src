# $FreeBSD: stable/11/stand/i386/btx/btxldr/Makefile 329183 2018-02-12 20:51:28Z kevans $

.include <bsd.init.mk>

PROG=	btxldr
INTERNALPROG=
SRCS=	btxldr.S

CFLAGS+=-DLOADER_ADDRESS=${LOADER_ADDRESS}
CFLAGS+=-I${BOOTSRC}/i386/common

.if defined(BTXLDR_VERBOSE)
CFLAGS+=-DBTXLDR_VERBOSE
.endif

ORG=${LOADER_ADDRESS}
LDFLAGS+=${LDFLAGS_BIN}

.include <bsd.prog.mk>

# XXX: clang integrated-as doesn't grok .codeNN directives yet
CFLAGS.btxldr.S=	${CLANG_NO_IAS}
