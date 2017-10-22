# $FreeBSD: release/10.0.0/usr.bin/nc/Makefile 214047 2010-10-19 00:01:53Z delphij $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

WARNS?=	2

.include <bsd.prog.mk>
