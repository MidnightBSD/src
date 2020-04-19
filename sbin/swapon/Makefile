#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: stable/11/sbin/swapon/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	swapon
MAN=	swapon.8
LINKS=	${BINDIR}/swapon ${BINDIR}/swapoff
LINKS+=	${BINDIR}/swapon ${BINDIR}/swapctl
MLINKS=	swapon.8 swapoff.8
MLINKS+=swapon.8 swapctl.8

LIBADD=	util

.include <bsd.prog.mk>
