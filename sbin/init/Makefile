#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/sbin/init/Makefile,v 1.32 2005/01/11 14:34:29 delphij Exp $
# $MidnightBSD: src/sbin/init/Makefile,v 1.4 2007/04/07 02:17:03 laffer1 Exp $

PROG=	init
MAN=	init.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

NO_SHARED?=	YES

.include <bsd.prog.mk>
