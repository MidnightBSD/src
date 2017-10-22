#	$FreeBSD: release/10.0.0/usr.bin/bc/Makefile 249657 2013-04-19 19:45:00Z ed $
#	$OpenBSD: Makefile,v 1.4 2006/06/30 19:02:28 otto Exp $

PROG=	bc
SRCS=	bc.y scan.l
CFLAGS+= -I. -I${.CURDIR}

DPADD=	${LIBEDIT} ${LIBTERMCAP}
LDADD=	-ledit -ltermcap

NO_WMISSING_VARIABLE_DECLARATIONS=

FILES+=	bc.library
FILESDIR=${SHAREDIR}/misc

.include <bsd.prog.mk>
