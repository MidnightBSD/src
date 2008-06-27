# $MidnightBSD: src/usr.bin/pcc/cc/Makefile,v 1.6 2008/06/25 19:49:02 laffer1 Exp $

PROG=	pcc
SRCS=	cc.c
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"${libexecdir}\"
CFLAGS+=	-I${.CURDIR}/..
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

BINDIR=	/usr/bin
#MANDIR= /usr/local/man/man
#MAN=	cc.1
NO_MAN=	
#MLINKS+=	cc.1 pcc.1

.include <bsd.prog.mk>
