# $FreeBSD: release/10.0.0/tools/regression/geom/MdLoad/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=	MdLoad
DPADD=	${LIBSBUF} ${LIBBSDXML}
LDADD=	-lsbuf -lbsdxml

NO_MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
