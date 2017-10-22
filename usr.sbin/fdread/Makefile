# $FreeBSD: release/7.0.0/usr.sbin/fdread/Makefile 139905 2005-01-08 15:46:06Z delphij $

PROG=	fdread
SRCS=	fdread.c fdutil.c

WARNS?=	6

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
