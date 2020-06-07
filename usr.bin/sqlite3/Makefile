# $MidnigtBSD: src/usr.bin/telnet/Makefile,v 1.28 2005/06/10 06:12:51 des Exp $

SQLITEDIR=	${SRCTOP}/contrib/sqlite3
.PATH:		${SQLITEDIR}

CFLAGS+=	-DHAVE_READLINE=1 -DSQLITE_ENABLE_FTS3

WARNS?=		0

PROG=		sqlite3

SRCS=		shell.c
		
LIBADD+=	sqlite3 readline

.include <bsd.prog.mk>
