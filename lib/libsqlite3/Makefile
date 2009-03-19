#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD: src/lib/libsqlite3/Makefile,v 1.4 2008/04/28 05:40:02 laffer1 Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} 

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h

LDADD=		-lpthread

.include <bsd.lib.mk>
