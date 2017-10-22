#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: release/10.0.0/sbin/ping/Makefile 161526 2006-08-22 08:03:01Z ru $

PROG=	ping
MAN=	ping.8
BINOWN=	root
BINMODE=4555
WARNS?=	2
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+=-DIPSEC
DPADD+=	${LIBIPSEC}
LDADD+=	-lipsec
.endif

.include <bsd.prog.mk>
