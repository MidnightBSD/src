# $MidnightBSD$
# $FreeBSD: stable/10/sbin/fdisk_pc98/Makefile 274008 2014-11-03 05:35:45Z nyan $

PROG=	fdisk
SRCS=	fdisk.c geom_pc98_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD+=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD+=	-lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
