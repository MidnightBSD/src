# $FreeBSD: stable/11/kerberos5/usr.bin/kgetcred/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	kgetcred
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken
LIBADD=	krb5 roken asn1 vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
