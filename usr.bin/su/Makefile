#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: release/10.0.0/usr.bin/su/Makefile 226428 2011-10-16 09:09:33Z ed $

.include <bsd.own.mk>

PROG=	su

WARNS?=	5

DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}

.if ${MK_AUDIT} != "no"
CFLAGS+= -DUSE_BSM_AUDIT
DPADD+=  ${LIBBSM}
LDADD+=  -lbsm
.endif

BINOWN=	root
BINMODE=4555
PRECIOUSPROG=

.include <bsd.prog.mk>
