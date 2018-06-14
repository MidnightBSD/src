# $MidnightBSD$
# $FreeBSD: stable/10/lib/libalias/modules/Makefile.inc 313581 2017-02-11 05:45:00Z ngie $

.PATH: ${SRCTOP}/sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
