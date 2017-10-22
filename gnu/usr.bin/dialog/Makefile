# Makefile for dialog
# $FreeBSD: release/7.0.0/gnu/usr.bin/dialog/Makefile 156813 2006-03-17 18:54:44Z ru $

.include <bsd.own.mk>

.if ${MK_EXAMPLES} != "no"
SUBDIR=	TESTS
.endif

PROG=   dialog

CFLAGS+= -Wall -Wstrict-prototypes

DPADD=	$(LIBDIALOG) $(LIBNCURSES)
LDADD=	-ldialog -lncurses

.include <bsd.prog.mk>
