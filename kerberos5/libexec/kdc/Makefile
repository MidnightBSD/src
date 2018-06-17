# $MidnightBSD$
# $FreeBSD: stable/10/kerberos5/libexec/kdc/Makefile 287428 2015-09-03 11:33:33Z ume $

PROG=	kdc
MAN=	kdc.8

SRCS=	config.c \
	connect.c \
	announce.c \
	main.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kdc ${LDAPCFLAGS}
DPADD=	${LIBKDC} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBASN1} \
	${LIBCRYPTO} ${LIBCRYPT} ${LIBVERS}
LDADD=	-lkdc -lhdb -lkrb5 -lroken -lasn1 -lcrypto -lcrypt ${LIBVERS}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
