# $FreeBSD: stable/11/usr.sbin/fdcontrol/Makefile 319119 2017-05-29 10:15:41Z ngie $

.PATH: ${.CURDIR:H}/fdread

PROG=	fdcontrol
SRCS=	fdcontrol.c fdutil.c
CFLAGS+= -I${.CURDIR:H}/fdread
MAN=	fdcontrol.8

.if ${MACHINE} == "pc98"
CFLAGS+= -DPC98
.endif

.include <bsd.prog.mk>
