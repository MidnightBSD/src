# $MidnigtBSD: src/usr.bin/telnet/Makefile,v 1.28 2005/06/10 06:12:51 des Exp $

SQLITEDIR=	${SRCTOP}/contrib/sqlite3
.PATH:		${SQLITEDIR}

CFLAGS+=	-DSQLITE_ENABLE_FTS3
# .archive needs zlib; .session needs the session extension, which
# lib/libsqlite3 also enables.
CFLAGS+=	-DSQLITE_HAVE_ZLIB -DSQLITE_ENABLE_SESSION
# Line editing through libedit's readline-compatible API.  With both
# set, shell.c includes <readline/readline.h>, found here.
CFLAGS+=	-DHAVE_READLINE=1 -DHAVE_EDITLINE=1 \
		-I${SYSROOT:U${DESTDIR}}/${INCLUDEDIR}/edit

WARNS?=		0

PROG=		sqlite3

SRCS=		shell.c

LIBADD+=	sqlite3
LIBADD+=	m
LIBADD+=	edit z

.include <bsd.prog.mk>
