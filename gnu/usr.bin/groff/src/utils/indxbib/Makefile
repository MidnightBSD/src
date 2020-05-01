# $FreeBSD: stable/11/gnu/usr.bin/groff/src/utils/indxbib/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG_CXX=	indxbib
SRCS=		indxbib.cpp signal.c
DPADD=		${LIBBIB} ${LIBGROFF}
LDADD=		${LIBBIB} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	${MAN}
FILES=		${DIST_DIR}/eign
FILESDIR=	${SHAREDIR}/dict/

.include <bsd.prog.mk>
