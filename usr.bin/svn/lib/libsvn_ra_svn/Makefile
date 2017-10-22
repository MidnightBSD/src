# $FreeBSD: release/10.0.0/usr.bin/svn/lib/libsvn_ra_svn/Makefile 251886 2013-06-18 02:53:45Z peter $

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_ra_svn

INTERNALLIB=	yes
LIB=		svn_ra_svn

SRCS=		client.c cram.c cyrus_auth.c deprecated.c editorp.c \
		internal_auth.c marshal.c streams.c version.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
