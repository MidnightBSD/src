# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/unbound/checkconf/Makefile 255597 2013-09-15 14:51:23Z des $

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns
UNBOUNDDIR= ${SRCTOP}/contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/util ${UNBOUNDDIR}/doc

PROG=	unbound-checkconf
SRCS=	unbound-checkconf.c worker_cb.c
CFLAGS=	-I${UNBOUNDDIR} -I${LDNSDIR}
CFLAGS+=	-I${.CURDIR:H} -I${.CURDIR}
DPADD=	${LIBUNBOUND} ${LIBLDNS} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD=	-lunbound -lldns -lutil -lssl -lcrypto -lpthread
USEPRIVATELIB=	ldns
MAN=	unbound-checkconf.8

.include <bsd.prog.mk>
