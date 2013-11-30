# $MidnigtBSD: src/usr.bin/telnet/Makefile,v 1.28 2005/06/10 06:12:51 des Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

CFLAGS+=	-DHAVE_READLINE=1

DPADD=		${LIBSQLITE3} ${LIBREADLINE}
LDADD=		-lsqlite3 -lreadline

PROG=		sqlite3

SRCS=		shell.c
		

.include <bsd.prog.mk>
