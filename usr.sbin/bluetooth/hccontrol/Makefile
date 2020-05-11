# $Id: Makefile,v 1.7 2003/08/14 20:06:17 max Exp $
# $FreeBSD: stable/11/usr.sbin/bluetooth/hccontrol/Makefile 281680 2015-04-18 04:53:17Z takawata $

PROG=		hccontrol
MAN=		hccontrol.8
SRCS=		send_recv.c link_policy.c link_control.c le.c\
		host_controller_baseband.c info.c status.c node.c hccontrol.c \
		util.c
WARNS?=		2

LIBADD=		bluetooth

.include <bsd.prog.mk>
