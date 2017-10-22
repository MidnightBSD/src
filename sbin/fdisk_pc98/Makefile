# $FreeBSD: release/10.0.0/sbin/fdisk_pc98/Makefile 148062 2005-07-15 15:31:19Z nyan $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD	+=	${LIBGEOM}
LDADD	+=	-lgeom

.include <bsd.prog.mk>
