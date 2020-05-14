# $FreeBSD: stable/11/usr.bin/svn/lib/libsvn_diff/Makefile 289180 2015-10-12 09:53:55Z peter $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_diff

INTERNALLIB=	yes
LIB=		svn_diff

SRCS=		binary_diff.c deprecated.c diff.c diff3.c diff4.c \
		diff_file.c diff_memory.c diff_tree.c lcs.c parse-diff.c \
		token.c util.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>

