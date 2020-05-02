# $FreeBSD: stable/11/kerberos5/libexec/kfd/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	kfd
MAN=	kfd.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
LIBADD=	krb5 roken vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
