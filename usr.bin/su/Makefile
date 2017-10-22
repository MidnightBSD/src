#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: stable/9/usr.bin/su/Makefile 201386 2010-01-02 10:27:05Z ed $

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
