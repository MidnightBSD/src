# $FreeBSD: stable/9/usr.sbin/authpf/Makefile 201390 2010-01-02 11:07:44Z ed $

.PATH:	${.CURDIR}/../../contrib/pf/authpf
.PATH:	${.CURDIR}/../../contrib/pf/pfctl

PROG=	authpf
MAN=	authpf.8
BINOWN=	root
BINGRP= authpf
BINMODE= 6555

SRCS=   authpf.c

CFLAGS+= -I${.CURDIR}/../../contrib/pf/pfctl

# XXX ALTQ:
CFLAGS+= -DENABLE_ALTQ

LDADD+= -lm -lmd -lutil
DPADD+= ${LIBM} ${LIBMD} ${LIBUTIL}

WARNS?=	3

.include <bsd.prog.mk>
