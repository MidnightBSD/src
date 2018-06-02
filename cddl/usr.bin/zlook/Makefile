# $MidnightBSD$
# $FreeBSD: stable/10/cddl/usr.bin/zlook/Makefile 321270 2017-07-20 01:01:16Z ngie $

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/cmd/zlook

PROG=	zlook
MAN=

WARNS?=	0
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

.include <bsd.prog.mk>
