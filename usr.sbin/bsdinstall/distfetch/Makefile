# $FreeBSD: release/10.0.0/usr.sbin/bsdinstall/distfetch/Makefile 250511 2013-05-11 13:46:05Z antoine $

BINDIR= /usr/libexec/bsdinstall
PROG=	distfetch
DPADD=	${LIBFETCH} ${LIBNCURSESW} ${LIBDIALOG} ${LIBM}
LDADD=	-lfetch -lncursesw -ldialog -lm

WARNS?=	6
NO_MAN=	true

.include <bsd.prog.mk>
