# $FreeBSD: stable/11/lib/libalias/modules/Makefile.inc 313538 2017-02-10 07:32:40Z ngie $

.PATH: ${SRCTOP}/sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
