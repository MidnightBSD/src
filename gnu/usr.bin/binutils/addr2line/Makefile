# $MidnightBSD: src/gnu/usr.bin/binutils/addr2line/Makefile,v 1.2 2009/11/26 15:22:09 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/binutils/addr2line/Makefile,v 1.11 2004/06/16 07:09:33 obrien Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROG=	addr2line
SRCS=	addr2line.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
