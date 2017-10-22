# $FreeBSD: release/7.0.0/usr.sbin/tzsetup/Makefile 113091 2003-04-04 17:49:21Z obrien $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}

DPADD=	${LIBDIALOG} ${LIBNCURSES}
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
