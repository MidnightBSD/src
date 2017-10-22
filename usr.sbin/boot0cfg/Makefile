# $FreeBSD: stable/9/usr.sbin/boot0cfg/Makefile 229212 2012-01-01 21:38:58Z dim $

PROG=	boot0cfg
MAN=	boot0cfg.8

DPADD=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD=	-lgeom -lbsdxml -lsbuf

NO_WCAST_ALIGN=

.include <bsd.prog.mk>
