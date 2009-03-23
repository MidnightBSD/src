#	From: @(#)Makefile	8.2 (Berkeley) 12/15/93
# $MidnightBSD: src/lib/libsqlite3/Makefile,v 1.6 2009/03/19 15:42:58 ctriv Exp $

SQLITEDIR=	${.CURDIR}/../../contrib/sqlite3
.PATH:		${SQLITEDIR}

LIB=		sqlite3
SHLIB_MAJOR=	1

SRCS=		sqlite3.c
CFLAGS+=	-I${SQLITEDIR} 

WARNS?=		0

INCS=		${SQLITEDIR}/sqlite3.h

afterinstall: 
	${INSTALL} ${SHLINSTALLFLAGS} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${_INSTALLFLAGS} ${SQLITEDIR}/sqlite3.pc ${DESTDIR}/usr/libdata/pkgconfig/sqlite3.pc
	

.include <bsd.lib.mk>
