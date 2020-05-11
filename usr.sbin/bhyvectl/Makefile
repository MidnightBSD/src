#
# $FreeBSD: stable/11/usr.sbin/bhyvectl/Makefile 319119 2017-05-29 10:15:41Z ngie $
#

PROG=	bhyvectl
SRCS=	bhyvectl.c
PACKAGE=	bhyve

MAN=	bhyvectl.8

LIBADD=	vmmapi util

WARNS?=	3

CFLAGS+= -I${SRCTOP}/sys/amd64/vmm

.include <bsd.prog.mk>
