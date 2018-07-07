# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/host/Makefile 270776 2014-08-29 01:20:31Z gjb $

LDNSDIR=	${.CURDIR}/../../contrib/ldns
LDNSHOSTDIR=	${.CURDIR}/../../contrib/ldns-host

.PATH:		${LDNSHOSTDIR}

PROG=		host
SRCS=		ldns-host.c
MAN=		host.1
CLEANFILES+=	host.1

host.1: ldns-host.1
	sed -e 's/ldns-//gI' <${.ALLSRC} >${.TARGET} || \
		(rm -rf ${.TARGET} ; false)

CFLAGS+=	-I${LDNSDIR}
DPADD+=		${LIBLDNS} ${LIBCRYPTO}
LDADD+=		-lldns -lcrypto
USEPRIVATELIB=	ldns

.include <bsd.prog.mk>
