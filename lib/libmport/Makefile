# $MidnightBSD: src/lib/libmport/Makefile,v 1.4 2007/11/23 05:57:43 ctriv Exp $

LIB=		mport
SRCS=		archive.c plist.c create_pkg.c db.c util.c error.c \
		install_pkg.c inst_init.c version_cmp.c

INCS=		mport.h


CFLAGS+=	-I${.CURDIR}
WARNS?=	3
WFORMAT?=	1
SHLIB_MAJOR=	1

DPADD=	${LIBSQLITE3} ${LIBMD}
LDADD=	-lsqlite3 -lmd

.include <bsd.lib.mk>
