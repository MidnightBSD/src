# $MidnightBSD: src/usr.sbin/pkg_install/lib/Makefile,v 1.18 2004/10/24 15:33:07 ru Exp $

LIB=		mport
SRCS=		plist.c	create_pkg.c db_schema.c db_stub.c db_util.c util.c error.c install_pkg.c inst_init.c

INCS=		mport.h


CFLAGS+=	-I${.CURDIR}
WARNS?=	3
WFORMAT?=	1
SHLIB_MAJOR=	1

DPADD=	${LIBSQLITE3} ${LIBMD}
LDADD=	-lsqlite3 -lmd

.include <bsd.lib.mk>
