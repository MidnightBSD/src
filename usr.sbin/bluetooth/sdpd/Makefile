# $Id: Makefile,v 1.2 2013-01-01 17:41:47 laffer1 Exp $
# $MidnightBSD$

PROG=		sdpd
MAN=		sdpd.8
SRCS=		bgd.c dun.c ftrn.c gn.c irmc.c irmc_command.c lan.c log.c \
		main.c nap.c opush.c panu.c profile.c provider.c sar.c scr.c \
		sd.c server.c sp.c srr.c ssar.c ssr.c sur.c uuid.c

CFLAGS+=	-I${.CURDIR}
WARNS?=		2

.include <bsd.prog.mk>
