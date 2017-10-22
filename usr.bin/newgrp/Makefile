# $FreeBSD: release/10.0.0/usr.bin/newgrp/Makefile 137164 2004-11-03 18:01:21Z ru $

PROG=	newgrp
DPADD=	${LIBCRYPT} ${LIBUTIL}
LDADD=	-lcrypt -lutil

.if defined(ENABLE_SUID_NEWGRP)
BINMODE=	4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
