# $FreeBSD: stable/10/kerberos5/libexec/kpasswdd/Makefile 178828 2008-05-07 13:53:12Z dfr $

PROG=	kpasswdd
MAN=	kpasswdd.8
CFLAGS+=-I${KRB5DIR}/lib/roken -I../../lib/libhdb ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
