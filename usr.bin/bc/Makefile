#	$FreeBSD: stable/9/usr.bin/bc/Makefile 203498 2010-02-04 18:43:05Z delphij $
#	$OpenBSD: Makefile,v 1.4 2006/06/30 19:02:28 otto Exp $

PROG=	bc
SRCS=	bc.y scan.l
CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

FILES+=	bc.library
FILESDIR=${SHAREDIR}/misc

.include <bsd.prog.mk>
