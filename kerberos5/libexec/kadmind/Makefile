# $FreeBSD: stable/10/kerberos5/libexec/kadmind/Makefile 236337 2012-05-30 22:21:25Z obrien $

PROG=	kadmind
MAN=	kadmind.8
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBGSSAPI} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} \
	${LIBASN1} ${LIBVERS} ${LIBCRYPTO} ${LIBCRYPT} ${LDAPDPADD}
LDADD=	-lkadm5srv -lgssapi -lhdb -lkrb5 -lroken \
	-lasn1 ${LIBVERS} -lcrypto -lcrypt ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
