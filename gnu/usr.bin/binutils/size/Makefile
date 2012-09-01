# $MidnightBSD: src/gnu/usr.bin/binutils/size/Makefile,v 1.2 2009/11/26 15:22:10 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/binutils/size/Makefile,v 1.12 2004/06/16 07:09:43 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	size
SRCS=	size.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
