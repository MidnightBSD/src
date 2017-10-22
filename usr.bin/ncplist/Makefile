# $FreeBSD: stable/9/usr.bin/ncplist/Makefile 229203 2012-01-01 20:53:33Z dim $

PROG=	ncplist
NO_WCAST_ALIGN=

WARNS?=	0

DPADD=	${LIBNCP} ${LIBIPX}
LDADD=	-lncp -lipx

.include <bsd.prog.mk>
