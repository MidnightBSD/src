# $FreeBSD: release/7.0.0/usr.sbin/pkg_install/delete/Makefile 133596 2004-08-12 20:06:01Z ru $

PROG=	pkg_delete
SRCS=	main.c perform.c

CFLAGS+= -I${.CURDIR}/../lib

WARNS?=	6
WFORMAT?=	1

DPADD=	${LIBINSTALL} ${LIBMD}
LDADD=	${LIBINSTALL} -lmd

.include <bsd.prog.mk>
