# $MidnightBSD$
# $FreeBSD: stable/10/sbin/mdconfig/Makefile 264996 2014-04-27 01:15:10Z jmmv $

.include <bsd.own.mk>

PROG=		mdconfig
MAN=		mdconfig.8

DPADD=	${LIBUTIL} ${LIBGEOM} ${LIBBSDXML} ${LIBSBUF} 
LDADD=	-lutil -lgeom -lbsdxml -lsbuf

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
