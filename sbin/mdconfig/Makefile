# $FreeBSD: release/10.0.0/sbin/mdconfig/Makefile 238202 2012-07-07 17:20:27Z eadler $

PROG=		mdconfig
MAN=		mdconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
