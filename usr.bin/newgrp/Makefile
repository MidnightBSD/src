# $FreeBSD: stable/11/usr.bin/newgrp/Makefile 275042 2014-11-25 14:29:10Z bapt $

PROG=	newgrp

LIBADD=	crypt util

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
