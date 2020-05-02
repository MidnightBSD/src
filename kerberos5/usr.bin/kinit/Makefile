# $FreeBSD: stable/11/kerberos5/usr.bin/kinit/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	kinit
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	kafs5 krb5 heimntlm roken crypto vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
