# $FreeBSD: stable/11/usr.sbin/yp_mkdb/Makefile 319191 2017-05-30 05:15:09Z ngie $

.PATH: ${SRCTOP}/libexec/ypxfr ${SRCTOP}/usr.sbin/ypserv

PROG=	yp_mkdb
MAN=	yp_mkdb.8
SRCS=	yp_mkdb.c yp_dblookup.c yp_dbwrite.c

CFLAGS+= -Dyp_error=warnx
CFLAGS+= -I${SRCTOP}/libexec/ypxfr -I${SRCTOP}/usr.sbin/ypserv

WARNS?=	1

.include <bsd.prog.mk>
