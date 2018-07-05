# $MidnightBSD$
#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/10/usr.bin/talk/Makefile 273499 2014-10-23 00:43:08Z ngie $

PROG=	talk
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_iface.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c
DPADD=	${LIBNCURSESW}
LDADD=	-lncursesw

.include <bsd.prog.mk>
