# $FreeBSD: stable/10/kerberos5/lib/libheimsqlite/Makefile 233294 2012-03-22 08:48:42Z stas $

LIB=	heimsqlite
SRCS=	sqlite3.c
DPADD=	${LIBPTHREAD}
LDADD=	-lpthread
CFLAGS+=-I${KRB5DIR}/lib/sqlite

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/sqlite
