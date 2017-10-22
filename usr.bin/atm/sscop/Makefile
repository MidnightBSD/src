# $FreeBSD: release/7.0.0/usr.bin/atm/sscop/Makefile 135771 2004-09-24 22:10:34Z trhodes $

CONTRIB= ${.CURDIR}/../../../contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
WARNS?=	6
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

DPADD=	${LIBBEGEMOT} ${LIBNETGRAPH} ${LIBNGATM}
LDADD=	-lbegemot -lnetgraph -lngatm

.include <bsd.prog.mk>
