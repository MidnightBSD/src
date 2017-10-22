#	From: @(#)Makefile	8.3 (Berkeley) 1/25/94
# $FreeBSD: release/10.0.0/usr.sbin/mountd/Makefile 158857 2006-05-23 17:10:17Z rodrigc $

PROG=	mountd
SRCS=	mountd.c getmntopts.c
MAN=	exports.5 netgroup.5 mountd.8

MOUNT=  ${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
WARNS?= 2

.PATH: ${MOUNT}

DPADD=	${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
