#	From: @(#)Makefile	8.3 (Berkeley) 1/25/94
# $FreeBSD: stable/11/usr.sbin/mountd/Makefile 319119 2017-05-29 10:15:41Z ngie $

PROG=	mountd
SRCS=	mountd.c getmntopts.c
MAN=	exports.5 netgroup.5 mountd.8

MOUNT=  ${SRCTOP}/sbin/mount
CFLAGS+= -I${MOUNT}
WARNS?= 2

.PATH: ${MOUNT}

LIBADD=	util

.include <bsd.prog.mk>
