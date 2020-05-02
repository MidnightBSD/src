# $FreeBSD: stable/11/kerberos5/usr.bin/string2key/Makefile 344200 2019-02-16 03:18:52Z nyan $

PROG=	string2key
MAN=	string2key.8
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/windc \
		-I${SRCTOP}/contrib/com_err
LIBADD=	krb5 roken crypto vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
