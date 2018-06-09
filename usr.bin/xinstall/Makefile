# $MidnightBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/xinstall/Makefile 291812 2015-12-04 18:48:08Z bdrewery $

PROG=		xinstall
PROGNAME=	install
SRCS=		xinstall.c getid.c
MAN=		install.1

.PATH:		${.CURDIR}/../../contrib/mtree
CFLAGS+=	-I${.CURDIR}/../../contrib/mtree
CFLAGS+=	-I${.CURDIR}/../../lib/libnetbsd

DPADD+=		${LIBMD}
LDADD+=		-lmd

.include <bsd.prog.mk>
