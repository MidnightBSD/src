# $FreeBSD: release/10.0.0/sbin/fdisk/Makefile 183296 2008-09-23 13:12:51Z ru $

PROG=	fdisk
SRCS=	fdisk.c geom_mbr_enc.c
WARNS?=	4
MAN=	fdisk.8

.PATH:	${.CURDIR}/../../sys/geom

DPADD+=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD+=	-lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>

test:	${PROG}
	sh ${.CURDIR}/runtest.sh
