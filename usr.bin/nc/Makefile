# $FreeBSD: release/7.0.0/usr.bin/nc/Makefile 141443 2005-02-07 08:44:34Z ru $

.PATH: ${.CURDIR}/../../contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LDADD=	-lipsec
DPADD=	${LIBIPSEC}

.include <bsd.prog.mk>
