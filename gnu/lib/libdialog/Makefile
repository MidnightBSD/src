# $MidnightBSD$

DIALOG=		${.CURDIR}/../../../contrib/dialog

.PATH:	${.CURDIR}/../../../contrib/dialog

LIB=		dialog
SHLIB_MAJOR=	7
SRCS=		argv.c arrows.c buttons.c calendar.c columns.c \
		dlg_keys.c editbox.c formbox.c guage.c \
		inputstr.c mixedform.c mixedgauge.c \
		mouse.c mousewget.c progressbox.c \
		tailbox.c timebox.c trace.c ui_getc.c util.c \
		version.c
SRCS+=		checklist.c fselect.c help.c inputbox.c menubox.c msgbox.c \
		pause.c prgbox.c rc.c textbox.c yesno.c
INCS=		dialog.h dlg_colors.h dlg_config.h dlg_keys.h
MAN=		dialog.3

CFLAGS+= 	-I${.CURDIR} -I${DIALOG} -D_BSD_TYPES -D__BSD_VISIBLE \
		-D_XOPEN_SOURCE_EXTENDED
WARNS?=		1

.include <bsd.lib.mk>
