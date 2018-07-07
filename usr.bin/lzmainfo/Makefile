# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/lzmainfo/Makefile 292588 2015-12-22 08:03:23Z delphij $

PROG=	lzmainfo

XZDIR=	${.CURDIR}/../../contrib/xz/src
LZMALIBDIR=	${.CURDIR}/../../lib/liblzma

.PATH: ${XZDIR}/lzmainfo
SRCS+=	lzmainfo.c

.PATH: ${XZDIR}/common
SRCS+=	tuklib_progname.c \
	tuklib_exit.c

CFLAGS+=	-DHAVE_CONFIG_H \
		-I${LZMALIBDIR} \
		-I${XZDIR}/common

DPADD=	${LIBLZMA} ${LIBPTHREAD}
LDADD=	-llzma -lpthread

.include <bsd.prog.mk>
