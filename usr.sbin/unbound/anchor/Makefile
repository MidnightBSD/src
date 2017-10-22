# $FreeBSD: release/10.0.0/usr.sbin/unbound/anchor/Makefile 255597 2013-09-15 14:51:23Z des $

# Vendor sources and generated files
LDNSDIR= ${.CURDIR}/../../../contrib/ldns
UNBOUNDDIR= ${.CURDIR}/../../../contrib/unbound
EXPATDIR= ${.CURDIR}/../../../contrib/expat

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/doc

PROG=	unbound-anchor
SRCS=	unbound-anchor.c
CFLAGS=	-I${UNBOUNDDIR} -I${LDNSDIR} -I${EXPATDIR}/lib
DPADD=	${LIBUNBOUND} ${LIBLDNS} ${LIBUTIL} ${LIBBSDXML} ${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD=	-lunbound -lldns -lutil -lbsdxml -lssl -lcrypto -lpthread
USEPRIVATELIB=	ldns
MAN=	unbound-anchor.8

.include <bsd.prog.mk>
