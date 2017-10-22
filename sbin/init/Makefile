#	@(#)Makefile	8.1 (Berkeley) 7/19/93
# $FreeBSD: release/10.0.0/sbin/init/Makefile 202194 2010-01-13 17:58:49Z ed $

PROG=	init
MAN=	init.8
PRECIOUSPROG=
INSTALLFLAGS=-b -B.bak
CFLAGS+=-DDEBUGSHELL -DSECURE -DLOGIN_CAP -DCOMPAT_SYSV_INIT
DPADD=	${LIBUTIL} ${LIBCRYPT}
LDADD=	-lutil -lcrypt

NO_SHARED?=	YES

.include <bsd.prog.mk>
