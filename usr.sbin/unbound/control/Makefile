# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/unbound/control/Makefile 295690 2016-02-17 11:36:45Z des $

# Vendor sources and generated files
LDNSDIR= ${.CURDIR}/../../../contrib/ldns
UNBOUNDDIR= ${.CURDIR}/../../../contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/doc

PROG=	unbound-control
SRCS=	unbound-control.c worker_cb.c
CFLAGS=	-I${UNBOUNDDIR} -I${LDNSDIR}
DPADD=	${LIBUNBOUND} ${LIBLDNS} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD=	-lunbound -lldns -lutil -lssl -lcrypto -lpthread
USEPRIVATELIB=	ldns
MAN=	unbound-control.8

.include <bsd.prog.mk>
