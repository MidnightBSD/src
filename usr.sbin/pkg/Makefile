# $FreeBSD: stable/9/usr.sbin/pkg/Makefile 245394 2013-01-13 22:37:45Z bapt $

PROG=	pkg
SRCS=	pkg.c dns_utils.c

NO_MAN=	yes
DPADD=	${LIBARCHIVE} ${LIBELF} ${LIBFETCH}
LDADD=	-larchive -lelf -lfetch

.include <bsd.prog.mk>
