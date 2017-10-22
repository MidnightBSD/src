# $FreeBSD: release/10.0.0/usr.bin/host/Makefile 255949 2013-09-30 17:23:45Z des $

LDNSDIR=	${.CURDIR}/../../contrib/ldns
LDNSHOSTDIR=	${.CURDIR}/../../contrib/ldns-host

.PATH:		${LDNSHOSTDIR}

PROG=		host
SRCS=		ldns-host.c
MAN=		host.1

host.1: ldns-host.1
	sed -e 's/ldns-//gI' <${.ALLSRC} >${.TARGET} || \
		(rm -rf ${.TARGET} ; false)

CFLAGS+=	-I${LDNSDIR}
DPADD+=		${LIBLDNS} ${LIBCRYPTO}
LDADD+=		-lldns -lcrypto
USEPRIVATELIB=	ldns

.include <bsd.prog.mk>
