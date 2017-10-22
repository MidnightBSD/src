# $FreeBSD: stable/9/usr.bin/xzdec/Makefile 207844 2010-05-10 07:47:31Z mm $

PROG=	xzdec

LINKS=	${BINDIR}/xzdec ${BINDIR}/lzdec

MLINKS= xzdec.1 lzmadec.1

XZDIR=	${.CURDIR}/../../contrib/xz/src
LZMALIBDIR=	${.CURDIR}/../../lib/liblzma

.PATH: ${XZDIR}/xzdec

SRCS=	xzdec.c

.PATH: ${XZDIR}/common

SRCS+=	tuklib_progname.c \
	tuklib_exit.c

WARNS?=	3

CFLAGS+=	-DHAVE_CONFIG_H \
		-I${LZMALIBDIR} \
		-I${XZDIR}/common

DPADD=	${LIBLZMA}
LDADD=	-llzma

.include <bsd.prog.mk>
