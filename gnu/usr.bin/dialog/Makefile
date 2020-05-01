# $FreeBSD: stable/11/gnu/usr.bin/dialog/Makefile 275077 2014-11-25 21:18:18Z bapt $

DIALOG=	${.CURDIR}/../../../contrib/dialog
PROG=   dialog

LIBADD=	dialog ncursesw m
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 6

.include <bsd.prog.mk>
