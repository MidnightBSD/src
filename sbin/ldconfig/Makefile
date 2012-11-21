# $MidnightBSD: src/sbin/ldconfig/Makefile,v 1.2 2008/11/11 22:53:47 laffer1 Exp $
# $FreeBSD: src/sbin/ldconfig/Makefile,v 1.20 2005/01/14 12:22:57 delphij Exp $

PROG=	ldconfig
SRCS=	elfhints.c ldconfig.c shlib.c support.c
LDDIR?= ${.CURDIR}/../../libexec/rtld-aout
CFLAGS+=-I${LDDIR} -DFREEBSD_AOUT
MAN=	ldconfig.8

.PATH: ${LDDIR}

.include <bsd.prog.mk>
