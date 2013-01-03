# $MidnightBSD: src/usr.sbin/sade/Makefile,v 1.3 2011/12/18 03:49:30 laffer1 Exp $

PROG=	sade
MAN=	sade.8
SRCS=	command.c config.c devices.c \
	disks.c dispatch.c dmenu.c \
	globals.c install.c \
	label.c main.c menus.c misc.c \
	msg.c system.c termcap.c \
	variable.c wizard.c
WARNS?=	3

CFLAGS+= -I${.CURDIR}/../../gnu/lib/libodialog -I.

DPADD=	${LIBODIALOG} ${LIBNCURSES} ${LIBUTIL} ${LIBDISK}
LDADD=	-lodialog -lncurses -lutil -ldisk

.include <bsd.prog.mk>
