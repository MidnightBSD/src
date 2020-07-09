# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/unbound/control/Makefile 295690 2016-02-17 11:36:45Z des $

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns
UNBOUNDDIR= ${SRCTOP}/contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/util ${UNBOUNDDIR}/doc

PROG=	unbound-control
SRCS=	ub_event.c unbound-control.c worker_cb.c
CFLAGS=	-I${UNBOUNDDIR} -I${LDNSDIR}
CFLAGS+=	-I${.CURDIR:H} -I${.CURDIR}
DPADD=	${LIBUNBOUND} ${LIBLDNS} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD=	-lunbound -lldns -lutil -lssl -lcrypto -lpthread
USEPRIVATELIB=	ldns
MAN=	unbound-control.8

.include <bsd.prog.mk>
