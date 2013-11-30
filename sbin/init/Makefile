#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: src/sbin/init/Makefile,v 1.32 2005/01/11 14:34:29 delphij Exp $
# $MidnightBSD: src/sbin/init/Makefile,v 1.3 2006/12/29 21:00:15 laffer1 Exp $

PROG=	init
MAN=	init.8
MLINKS=	init.8 securelevel.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
WARNS?=	3
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

NO_SHARED?=	YES

.include <bsd.prog.mk>
