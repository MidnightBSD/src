# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/unbound/daemon/Makefile 255597 2013-09-15 14:51:23Z des $

# Vendor sources and generated files
LDNSDIR= ${SRCTOP}/contrib/ldns
UNBOUNDDIR= ${SRCTOP}/contrib/unbound

.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/daemon ${UNBOUNDDIR}/util ${UNBOUNDDIR}/util/shm_side ${UNBOUNDDIR}/doc

PROG=	unbound
SRCS=	acl_list.c cachedump.c daemon.c remote.c shm_main.c stats.c \
	ub_event.c unbound.c worker.c
CFLAGS+=	-I${UNBOUNDDIR} -I${LDNSDIR}
CFLAGS+=	-I${.CURDIR:H} -I${.CURDIR}
DPADD=	${LIBUNBOUND} ${LIBLDNS} ${LIBUTIL} ${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD=	-lunbound -lldns -lutil -lssl -lcrypto -lpthread
USEPRIVATELIB=	ldns
MAN=	unbound.8 unbound.conf.5

.include <bsd.prog.mk>
