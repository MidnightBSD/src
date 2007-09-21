#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $FreeBSD: src/lib/libtelnet/Makefile,v 1.22 2004/12/21 10:16:00 ru Exp $


SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

DPADD=		${LIBPTHREAD}
LDADD=		-lpthread


SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR}

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h

.include <bsd.lib.mk>
