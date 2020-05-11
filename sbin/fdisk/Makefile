# $FreeBSD: stable/11/sbin/fdisk/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${SRCTOP}/sys/geom

LIBADD=	geom

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
