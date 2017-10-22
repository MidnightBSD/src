# $FreeBSD: stable/9/sbin/gvinum/Makefile 220370 2011-04-05 18:41:01Z obrien $

PROG=	gvinum
SRCS=	gvinum.c gvinum.h geom_vinum_share.c
MAN=	gvinum.8

WARNS?=	2
CFLAGS=	-I${.CURDIR}/../../sys -I${DESTDIR}/${INCLUDEDIR}/edit

DPADD=	${LIBEDIT} ${LIBTERMCAP} ${LIBDEVSTAT} ${LIBKVM} ${LIBGEOM}
LDADD=	-ledit -ltermcap -ldevstat -lkvm -lgeom

.PATH: ${.CURDIR}/../../sys/geom/vinum

.include <bsd.prog.mk>
