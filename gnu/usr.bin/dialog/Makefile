# $FreeBSD: stable/9/gnu/usr.bin/dialog/Makefile 217536 2011-01-18 14:58:12Z nwhitehorn $

DIALOG=	${.CURDIR}/../../../contrib/dialog
PROG=   dialog

DPADD=	$(LIBDIALOG) $(LIBNCURSES) $(LIBM)
LDADD=	-ldialog -lncursesw -lm
CFLAGS+= -I${.CURDIR} -I${DIALOG}
.PATH:	${DIALOG}

WARNS?= 3

.include <bsd.prog.mk>
