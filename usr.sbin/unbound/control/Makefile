# $FreeBSD: stable/11/usr.sbin/unbound/control/Makefile 356914 2020-01-20 13:46:09Z cy $

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns
UNBOUNDDIR= ${SRCTOP}/contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/smallapp ${UNBOUNDDIR}/util ${UNBOUNDDIR}/doc

PROG=	local-unbound-control
SRCS=	ub_event.c unbound-control.c worker_cb.c
CFLAGS+=	-I${UNBOUNDDIR} -I${LDNSDIR}
CFLAGS+=	-I${.CURDIR:H} -I${.CURDIR}
LIBADD=	unbound crypto ssl pthread
MAN=	local-unbound-control.8

.include <bsd.prog.mk>
