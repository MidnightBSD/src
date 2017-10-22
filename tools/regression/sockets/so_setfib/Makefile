# $FreeBSD: release/10.0.0/tools/regression/sockets/so_setfib/Makefile 231855 2012-02-17 03:25:26Z bz $

PROG=	so_setfib
NO_MAN=
WARNS?=	6

.ifdef INET6
CFLAGS+=	-DINET6
.endif
.ifdef INET
CFLAGS+=	-DINET
.endif

.include <bsd.prog.mk>
