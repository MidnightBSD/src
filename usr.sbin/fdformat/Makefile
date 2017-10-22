# $FreeBSD: release/10.0.0/usr.sbin/fdformat/Makefile 201390 2010-01-02 11:07:44Z ed $

.PATH: ${.CURDIR}/../fdread

PROG=	fdformat
SRCS=	fdformat.c fdutil.c

CFLAGS+= -I${.CURDIR}/../fdread

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
