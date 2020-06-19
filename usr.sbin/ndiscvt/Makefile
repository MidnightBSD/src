# $FreeBSD: stable/11/usr.sbin/ndiscvt/Makefile 352280 2019-09-13 07:21:06Z bapt $

.PATH:	${SRCTOP}/sys/compat/ndis

PROG=	ndiscvt
SRCS=	ndiscvt.c
SRCS+=	subr_pe.c
SRCS+=	inf.c inf-token.l inf-parse.y y.tab.h

MAN=	ndiscvt.8
MAN+=	ndisgen.8

WARNS?=	4
NO_WCAST_ALIGN=

YFLAGS+=-v

CFLAGS+=-I. -I${.CURDIR} -I${SRCTOP}/sys

CLEANFILES= y.output

FILES= windrv_stub.c
FILESDIR= ${SHAREDIR}/misc

SCRIPTS= ndisgen.sh

.include <bsd.prog.mk>
