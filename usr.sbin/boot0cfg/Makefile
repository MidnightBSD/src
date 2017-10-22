# $FreeBSD: release/10.0.0/usr.sbin/boot0cfg/Makefile 228656 2011-12-17 19:33:26Z dim $

PROG=	boot0cfg
MAN=	boot0cfg.8

DPADD=	${LIBGEOM} ${LIBBSDXML} ${LIBSBUF}
LDADD=	-lgeom -lbsdxml -lsbuf

NO_WCAST_ALIGN=

.include <bsd.prog.mk>
