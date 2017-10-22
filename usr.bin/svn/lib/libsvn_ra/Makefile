# $FreeBSD: release/10.0.0/usr.bin/svn/lib/libsvn_ra/Makefile 251886 2013-06-18 02:53:45Z peter $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_ra

INTERNALLIB=	yes
LIB=		svn_ra

SRCS=		compat.c debug_reporter.c deprecated.c editor.c \
		ra_loader.c util.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
