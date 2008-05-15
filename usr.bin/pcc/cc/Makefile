# $MidnightBSD: src/usr.bin/pcc/cc/Makefile,v 1.4 2008/05/15 05:38:35 laffer1 Exp $

PROG=	pcc
SRCS=	cc.c
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

BINDIR=	/usr/local/bin
MANDIR= /usr/local/man/man
MAN=	cc.1
#NO_MAN=	
MLINKS+=	cc.1 pcc.1

.include <bsd.prog.mk>
