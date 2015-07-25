# $FreeBSD: stable/10/kerberos5/usr.bin/kgetcred/Makefile 233294 2012-03-22 08:48:42Z stas $

PROG=	kgetcred
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkrb5 -lroken -lasn1 -lcrypto -lcrypt ${LIBVERS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
