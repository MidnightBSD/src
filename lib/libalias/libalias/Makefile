# $FreeBSD: stable/11/lib/libalias/libalias/Makefile 313538 2017-02-10 07:32:40Z ngie $

.PATH: ${SRCTOP}/sys/netinet/libalias

PACKAGE=lib${LIB}
LIB=	alias
SHLIBDIR?= /lib
SHLIB_MAJOR= 7
MAN=	libalias.3
SRCS=	alias.c alias_db.c alias_proxy.c alias_util.c alias_mod.c
INCS=	alias.h
NO_WERROR=

.include <bsd.lib.mk>
