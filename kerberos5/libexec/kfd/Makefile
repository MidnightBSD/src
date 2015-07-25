# $FreeBSD: stable/10/kerberos5/libexec/kfd/Makefile 236337 2012-05-30 22:21:25Z obrien $

PROG=	kfd
MAN=	kfd.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} \
	${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto -lcrypt \
	${LIBVERS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
