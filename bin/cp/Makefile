#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: stable/11/bin/cp/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	cp
SRCS=	cp.c utils.c
CFLAGS+= -DVM_AND_BUFFER_CACHE_SYNCHRONIZED -D_ACL_PRIVATE

.include <bsd.prog.mk>
