# $FreeBSD: stable/11/sbin/geom/class/Makefile.inc 319185 2017-05-30 04:17:22Z ngie $

SHLIBDIR?=${GEOM_CLASS_DIR}
SHLIB_NAME?=geom_${GEOM_CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${GEOM_CLASS}
MAN=	g${GEOM_CLASS}.8
SRCS+=	geom_${GEOM_CLASS}.c subr.c

NO_WMISSING_VARIABLE_DECLARATIONS=

CFLAGS+= -I${.CURDIR:H:H}

.include "../Makefile.inc"
