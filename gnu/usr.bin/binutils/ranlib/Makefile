# $MidnightBSD: src/gnu/usr.bin/binutils/ranlib/Makefile,v 1.2 2009/11/26 15:22:10 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/binutils/ranlib/Makefile,v 1.14 2004/12/21 09:59:41 ru Exp $

.include "../Makefile.inc0"
.include <bsd.own.mk>

.PATH: ${SRCDIR}/binutils ${SRCDIR}/binutils/doc

PROGNAME= gnu-ranlib
MAN=	gnu-ranlib.1
gnu-ranlib.1: ranlib.1
	cat ${.ALLSRC} > ${.TARGET}
CLEANFILES+= gnu-ranlib.1

PROG=	ranlib
SRCS=	ar.c is-ranlib.c
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${.CURDIR}/${RELTOP}/libbinutils
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
.if ${MK_SHARED_TOOLCHAIN} == "no"
NO_SHARED?= yes
.endif
DPADD=	${RELTOP}/libbinutils/libbinutils.a
DPADD+=	${RELTOP}/libbfd/libbfd.a
DPADD+=	${RELTOP}/libiberty/libiberty.a
LDADD=	${DPADD}

.include <bsd.prog.mk>
