#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD: src/lib/libsqlite3/Makefile,v 1.10 2011/08/06 02:22:58 laffer1 Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} -DSQLITE_ENABLE_FTS3 -DSQLITE_OS_UNIX=1 \
		-DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 \
		-DHAVE_STDLIB_H=1 -DHAVE_INTTYPES_H=1 -DREENTRANT=1 \
		-DSQLITE_THREADSAFE=1 -DSQLITE_ENABLE_RTREE=1

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h ${SQLITEDIR}/sqlite3ext.h

.if !defined(COMPAT_32BIT)
afterinstall: 
	${INSTALL} ${SHLINSTALLFLAGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${SQLITEDIR}/sqlite3.pc ${DESTDIR}/usr/libdata/pkgconfig/sqlite3.pc
.endif

.include <bsd.lib.mk>
