#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD$

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} -DSQLITE_ENABLE_FTS3 -DSQLITE_OS_UNIX=1 \
		-DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 \
		-DHAVE_STDLIB_H=1 -DHAVE_INTTYPES_H=1 -DREENTRANT=1 \
		-DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_RTREE=1 \
		-DSQLITE_ENABLE_FTS5 -DSQLITE_ENABLE_FTS4 \
		-DSQLITE_HAVE_ZLIB

WARNS?=		0

NO_MAN=

INCS=		${SQLITEDIR}/sqlite3.h ${SQLITEDIR}/sqlite3ext.h

DPADD=	${LIBPTHREAD} ${LIBM} ${LIBZ}
LDADD=	-lpthread -lm -lz

.if !defined(COMPAT_32BIT)
afterinstall: 
	${INSTALL} ${SHLINSTALLFLAGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${SQLITEDIR}/sqlite3.pc ${DESTDIR}/usr/libdata/pkgconfig/sqlite3.pc
.endif

.include <bsd.lib.mk>
