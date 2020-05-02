# $FreeBSD: stable/11/kerberos5/usr.bin/kf/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	kf
MAN=	kf.1
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
LIBADD=	krb5 roken vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
