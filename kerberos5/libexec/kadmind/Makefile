# $FreeBSD: stable/11/kerberos5/libexec/kadmind/Makefile 344200 2019-02-16 03:18:52Z nyan $

PROG=	kadmind
MAN=	kadmind.8
SRCS=	rpc.c \
	server.c \
	kadmind.c \
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${SRCTOP}/contrib/com_err ${LDAPCFLAGS}
LIBADD=	kadm5srv gssapi hdb krb5 roken vers
DPADD=	${LDAPDPADD}
LDADD=	${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kadmin
