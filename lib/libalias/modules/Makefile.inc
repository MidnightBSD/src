# $FreeBSD: release/10.0.0/lib/libalias/modules/Makefile.inc 163128 2006-10-08 14:02:00Z piso $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
