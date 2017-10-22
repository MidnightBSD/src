# $FreeBSD: stable/9/usr.bin/ncplogin/Makefile 90415 2002-02-08 22:31:43Z markm $

PROG=	ncplogin
MAN=	ncplogin.1 ncplogout.1

LINKS=	${BINDIR}/ncplogin ${BINDIR}/ncplogout

LDADD=	-lncp -lipx
DPADD=	${LIBNCP} ${LIBIPX}

.include <bsd.prog.mk>
