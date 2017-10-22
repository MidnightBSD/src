# $FreeBSD: release/10.0.0/tools/tools/net80211/wlanstats/Makefile 255487 2013-09-12 02:31:32Z hiren $

.include <bsd.compiler.mk>

PROG=	wlanstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	statfoo.c wlanstats.c main.c
.if ${COMPILER_TYPE} == "clang"
CFLAGS+= -fbracket-depth=512
.endif

.include <bsd.prog.mk>
