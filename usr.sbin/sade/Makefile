# $MidnightBSD: src/usr.sbin/sade/Makefile,v 1.2 2009/10/24 15:55:41 laffer1 Exp $
# $FreeBSD: src/usr.sbin/sade/Makefile,v 1.147 2006/08/09 11:25:10 netchild Exp $

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
