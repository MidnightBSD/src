# $FreeBSD: release/10.0.0/usr.sbin/tzsetup/Makefile 227934 2011-11-24 13:24:06Z fjoe $

PROG=	tzsetup
MAN=	tzsetup.8

CFLAGS+= -I${.CURDIR}/../../contrib/dialog -I.

WARNS?=	3

DPADD=	${LIBDIALOG} ${LIBNCURSESW} ${LIBM}
LDADD=	-ldialog -lncursesw -lm

.include <bsd.prog.mk>
