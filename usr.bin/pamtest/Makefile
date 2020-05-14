# $FreeBSD: stable/11/usr.bin/pamtest/Makefile 319186 2017-05-30 04:27:05Z ngie $

OPENPAM=	${SRCTOP}/contrib/openpam
.PATH: ${OPENPAM}/include ${OPENPAM}/bin/pamtest

PROG= pamtest
SRCS= pamtest.c

LIBADD=	pam

.include <bsd.prog.mk>
