#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD$

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} -DTHREADSAFE=0

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h

.include <bsd.lib.mk>
