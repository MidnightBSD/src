# $FreeBSD: stable/11/lib/librtld_db/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <bsd.own.mk>

PACKAGE=lib${LIB}
LIB=		rtld_db
SHLIB_MAJOR= 	2
MAN=		librtld_db.3

SRCS=		rtld_db.c
INCS=		rtld_db.h

CFLAGS+= -I${.CURDIR}
# Avoid circular dependency, we only need the libproc.h header here.
CFLAGS+= -I${.CURDIR:H}/libproc

.include <bsd.lib.mk>
