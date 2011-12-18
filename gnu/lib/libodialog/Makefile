# $MidnightBSD: src/gnu/lib/libdialog/Makefile,v 1.3 2008/11/11 15:56:59 laffer1 Exp $
# $FreeBSD: src/gnu/lib/libdialog/Makefile,v 1.45 2007/06/18 18:47:52 rafan Exp $

.include <bsd.own.mk>

LIB=    odialog
NOMAN=

SHLIB_MAJOR= 7
SRCS=   kernel.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	lineedit.c radiolist.c textbox.c yesno.c prgbox.c raw_popen.c \
	fselect.c ui_objects.c dir.c notify.c help.c gauge.c tree.c

CFLAGS+= -I${.CURDIR} -Wall -Wstrict-prototypes -DLOCALE

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

.include <bsd.lib.mk>
