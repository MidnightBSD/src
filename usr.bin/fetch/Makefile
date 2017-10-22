# $FreeBSD: release/10.0.0/usr.bin/fetch/Makefile 240496 2012-09-14 13:00:43Z des $

.include <bsd.own.mk>

PROG=		fetch
CSTD?=		c99
.if ${MK_OPENSSL} != "no"
DPADD=		${LIBFETCH} ${LIBSSL} ${LIBCRYPTO}
LDADD=		-lfetch -lssl -lcrypto
.else
DPADD=		${LIBFETCH} ${LIBMD}
LDADD=		-lfetch -lmd
.endif

.include <bsd.prog.mk>
