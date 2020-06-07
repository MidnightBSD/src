# $FreeBSD: stable/11/usr.bin/atm/sscop/Makefile 314009 2017-02-21 02:24:45Z ngie $

CONTRIB= ${SRCTOP}/contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

LIBADD=	begemot ngatm

.include <bsd.prog.mk>
