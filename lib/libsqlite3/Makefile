#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD: src/lib/libsqlite3/Makefile,v 1.5 2009/03/19 15:07:58 ctriv Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} 

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h

.include <bsd.lib.mk>
