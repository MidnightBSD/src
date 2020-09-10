# $FreeBSD: stable/11/tools/bus_space/Python/Makefile 284228 2015-06-10 16:00:09Z marcel $

SHLIB_NAME=	bus.so
SRCS=		lang.c

CFLAGS+= -I${.CURDIR}/.. -I/usr/local/include/python2.7
LDFLAGS+= -L/usr/local/lib -lpython2.7

.include <bsd.lib.mk>
