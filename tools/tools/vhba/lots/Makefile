# $FreeBSD: release/10.0.0/tools/tools/vhba/lots/Makefile 208926 2010-06-08 22:26:47Z mjacob $
KMOD=	vlots
SRCS=	vhba_lots.c vhba.c
CFLAGS += -I${.CURDIR}/.. -DVHBA_MOD=\"vlots\"
VPATH=	${.CURDIR}/..

.include <bsd.kmod.mk>
