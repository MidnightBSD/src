# Makefile for libdialog
# $FreeBSD: stable/9/gnu/lib/libodialog/Makefile 217309 2011-01-12 14:55:02Z nwhitehorn $

.include <bsd.own.mk>

LIB=    odialog
#MAN=	NOMAN

SHLIB_MAJOR= 7
SRCS=   kernel.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
	lineedit.c radiolist.c textbox.c yesno.c prgbox.c raw_popen.c \
	fselect.c ui_objects.c dir.c notify.c help.c gauge.c tree.c

CFLAGS+= -I${.CURDIR} -Wall -Wstrict-prototypes -DLOCALE

DPADD=	${LIBNCURSES}
LDADD=	-lncurses

.if ${MK_HTML} != "no"
FILES=	${EXAMPLES:C;^;${.CURDIR}/TESTS/;}
FILESDIR= ${SHAREDIR}/examples/libdialog
.endif

.include <bsd.lib.mk>
