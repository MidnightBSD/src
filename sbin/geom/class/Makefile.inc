# $FreeBSD: stable/9/sbin/geom/class/Makefile.inc 216468 2010-12-15 23:24:34Z obrien $

SHLIBDIR?=${GEOM_CLASS_DIR}
SHLIB_NAME?=geom_${GEOM_CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${GEOM_CLASS}
MAN=	g${GEOM_CLASS}.8
SRCS+=	geom_${GEOM_CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
