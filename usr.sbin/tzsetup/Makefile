# $FreeBSD: stable/9/usr.sbin/tzsetup/Makefile 247722 2013-03-03 18:59:59Z dteske $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../contrib/dialog -I.

WARNS?=	3

DPADD=	${LIBDIALOG} ${LIBNCURSESW} ${LIBM}
LDADD=	-ldialog -lncursesw -lm

.include <bsd.prog.mk>
