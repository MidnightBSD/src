# $FreeBSD: stable/9/sbin/mdconfig/Makefile 240229 2012-09-08 03:11:02Z eadler $

PROG=		mdconfig
MAN=		mdconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.include <bsd.prog.mk>
