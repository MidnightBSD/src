# $FreeBSD: stable/11/kerberos5/usr.bin/ksu/Makefile 275022 2014-11-25 09:57:42Z bapt $

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
PRECIOUSPROG=
.endif
MAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	kafs5 krb5 roken vers crypto crypt

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
