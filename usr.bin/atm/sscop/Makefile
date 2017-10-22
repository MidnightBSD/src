# $FreeBSD: release/10.0.0/usr.bin/atm/sscop/Makefile 201386 2010-01-02 10:27:05Z ed $

CONTRIB= ${.CURDIR}/../../../contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

DPADD=	${LIBBEGEMOT} ${LIBNETGRAPH} ${LIBNGATM}
LDADD=	-lbegemot -lnetgraph -lngatm

.include <bsd.prog.mk>
