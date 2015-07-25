# $FreeBSD: stable/10/kerberos5/usr.bin/kdestroy/Makefile 233294 2012-03-22 08:48:42Z stas $

PROG=	kdestroy
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHEIMNTLM} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lkafs5 -lkrb5 -lheimntlm -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt
MAN=	kdestroy.1

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
