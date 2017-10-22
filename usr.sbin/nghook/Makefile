# $FreeBSD: release/7.0.0/usr.sbin/nghook/Makefile 126178 2004-02-23 20:25:27Z johan $
# $Whistle: Makefile,v 1.4 1999/01/16 04:44:33 archie Exp $

PROG=	nghook
MAN=	nghook.8
SRCS=	main.c
WARNS?=	6

DPADD=	${LIBNETGRAPH}
LDADD=	-lnetgraph

.include <bsd.prog.mk>
