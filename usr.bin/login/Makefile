# 	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/usr.bin/login/Makefile,v 1.48 2004/11/03 18:01:19 ru Exp $
# $MidnightBSD: src/usr.bin/login/Makefile,v 1.3 2006/12/29 22:27:52 laffer1 Exp $

PROG=	login
SRCS=	login.c login_fbtab.c
CFLAGS+=-DLOGALL -Wall -std=c99
DPADD=	${LIBUTIL} ${LIBPAM}
LDADD=	-lutil ${MINUSLPAM}
.if !defined(NO_SETUID_LOGIN)
BINOWN=	root
BINMODE=4555
PRECIOUSPROG=
.endif

.include <bsd.prog.mk>
