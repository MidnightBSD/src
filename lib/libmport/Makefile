# $MidnightBSD: src/lib/libmport/Makefile,v 1.3 2007/11/22 08:00:32 ctriv Exp $

LIB=		mport
SRCS=		plist.c	create_pkg.c db_schema.c db_stub.c db_util.c util.c error.c install_pkg.c inst_init.c version_cmp.c

INCS=		mport.h


CFLAGS+=	-I${.CURDIR}
WARNS?=	3
WFORMAT?=	1
SHLIB_MAJOR=	1

DPADD=	${LIBSQLITE3} ${LIBMD}
LDADD=	-lsqlite3 -lmd

.include <bsd.lib.mk>
