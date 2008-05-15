# $MidnightBSD: src/usr.bin/pcc/cc/Makefile,v 1.2 2008/05/14 04:25:56 laffer1 Exp $

PROG=	cc
PREFIX=	/usr/local
BINDIR=	${PREFIX}/bin
MANDIR=	${PREFIX}/man/man
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${PREFIX}\"
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

MLINKS+=	cc.1 pcc.1

.include <bsd.prog.mk>
