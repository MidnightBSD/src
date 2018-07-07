# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/atm/sscop/Makefile 314008 2017-02-21 02:24:31Z ngie $

CONTRIB= ${SRCTOP}/contrib/ngatm/sscop

.PATH:	${CONTRIB}

PROG=	sscop
SRCS=	common.c sscop_main.c
CFLAGS+= -I${CONTRIB} -DUSE_LIBBEGEMOT

DPADD=	${LIBBEGEMOT} ${LIBNETGRAPH} ${LIBNGATM}
LDADD=	-lbegemot -lnetgraph -lngatm

.include <bsd.prog.mk>
