# $MidnightBSD: src/usr.bin/pcc/cc/Makefile,v 1.3 2008/05/15 04:53:32 laffer1 Exp $

PROG=	pcc
SRCS=	cc.c
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

BINDIR=	/usr/bin
#MAN=	cc.1
NO_MAN=	
#MLINKS+=	cc.1 pcc.1

.include <bsd.prog.mk>
