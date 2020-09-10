# $FreeBSD: stable/11/tools/regression/geom/MdLoad/Makefile 291738 2015-12-04 03:17:47Z bdrewery $

PROG=	MdLoad
LIBADD=	sbuf bsdxml

MAN=


WARNS?=	4
.include "bsd.prog.mk"

test:	${PROG}
	./${PROG} md34 ../Data/disk.critter.ad0.xml
