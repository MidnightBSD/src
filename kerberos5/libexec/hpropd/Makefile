# $FreeBSD: stable/11/kerberos5/libexec/hpropd/Makefile 344200 2019-02-16 03:18:52Z nyan $

PROG=	hpropd
MAN=	hpropd.8
CFLAGS+=-I${KRB5DIR}/lib/roken -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 \
	-I${KRB5DIR}/kdc -I${SRCTOP}/contrib/com_err ${LDAPCFLAGS}
LIBADD=	hdb krb5 roken vers
DPADD=	${LDAPDPADD}
LDADD=	${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
