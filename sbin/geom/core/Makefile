# $FreeBSD: stable/11/sbin/geom/core/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH: ${.CURDIR:H}/misc

PACKAGE=runtime
PROG=	geom
MAN=	geom.8
SRCS=	geom.c subr.c

NO_SHARED=NO

CFLAGS+= -DGEOM_CLASS_DIR=\"${GEOM_CLASS_DIR}\"
CFLAGS+= -I${SRCTOP}/sys -I${.CURDIR} -I${.CURDIR:H}

LIBADD=	geom util

.include <bsd.prog.mk>
