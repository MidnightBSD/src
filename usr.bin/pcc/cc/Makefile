# $MidnightBSD: src/usr.bin/pcc/cc/Makefile,v 1.8 2008/07/03 23:57:44 laffer1 Exp $

PROG=	pcc
SRCS=	cc.c
TARGOS=	midnightbsd
TARGMACH=	${MACHINE_ARCH}

CFLAGS+=	-DLIBEXECDIR=\"/usr/libexec/\"
CFLAGS+=	-I${.CURDIR}/.. -I${.CURDIR}/../mip/
CFLAGS+=	-Dmach_${TARGMACH} -Dos_${TARGOS}

BINDIR=	/usr/bin
#MANDIR= /usr/local/man/man
#MAN=	cc.1
NO_MAN=	
#MLINKS+=	cc.1 pcc.1

.include <bsd.prog.mk>
