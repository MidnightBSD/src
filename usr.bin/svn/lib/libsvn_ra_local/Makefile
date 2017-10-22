# $FreeBSD: release/10.0.0/usr.bin/svn/lib/libsvn_ra_local/Makefile 251886 2013-06-18 02:53:45Z peter $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_ra_local

INTERNALLIB=	yes
LIB=		svn_ra_local

SRCS=		ra_plugin.c split_url.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
