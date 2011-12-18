# $MidnightBSD$

.PATH: ${.CURDIR}/../../../contrib/dialog

PROG=	dialog

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../contrib/dialog

WARNS?=	3

DPADD=	$(LIBDIALOG) $(LIBNCURSES) $(LIBM)
LDADD=	-ldialog -lncursesw -lm

.include <bsd.prog.mk>
