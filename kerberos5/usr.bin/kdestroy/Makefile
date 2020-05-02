# $FreeBSD: stable/11/kerberos5/usr.bin/kdestroy/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	kdestroy
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	kafs5 krb5 roken vers
MAN=	kdestroy.1

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
