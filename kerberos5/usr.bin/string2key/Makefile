# $FreeBSD: release/10.0.0/kerberos5/usr.bin/string2key/Makefile 236337 2012-05-30 22:21:25Z obrien $

PROG=	string2key
MAN=	string2key.8
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/windc
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lhdb -lkrb5 -lroken -lasn1 -lcrypto -lcrypt ${LIBVERS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
