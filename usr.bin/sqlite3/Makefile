# $FreeBSD: src/usr.bin/telnet/Makefile,v 1.28 2005/06/10 06:12:51 des Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

PROG=		sqlite3

SRCS=		shell.c
		
DPADD=		${LIBPTHREAD} ${LIBSQLITE}
LDADD=		-lpthread -lsqlite3


.include <bsd.prog.mk>
