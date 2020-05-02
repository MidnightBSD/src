# $FreeBSD: stable/11/kerberos5/usr.bin/kpasswd/Makefile 287216 2015-08-27 17:16:18Z ume $

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	hdb krb5 roken vers crypto
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
