# $FreeBSD: stable/10/kerberos5/usr.bin/ksu/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
PRECIOUSPROG=
.endif
MAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
