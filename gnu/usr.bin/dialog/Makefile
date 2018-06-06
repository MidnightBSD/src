# $MidnightBSD$
# $FreeBSD: stable/10/gnu/usr.bin/dialog/Makefile 262643 2014-03-01 03:09:16Z brooks $

DIALOG=	${.CURDIR}/../../../contrib/dialog
PROG=   dialog

DPADD=	${LIBDIALOG} ${LIBM}
LDADD=	-ldialog -lm
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 6

.include <bsd.own.mk>

.if ${MK_NCURSESW} == "no"
DPADD+=	${LIBNCURSES}
LDADD+=	-lncurses
.else
DPADD+=	${LIBNCURSESW}
LDADD+=	-lncursesw
.endif

.include <bsd.prog.mk>
