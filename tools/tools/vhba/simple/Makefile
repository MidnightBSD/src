# $FreeBSD: release/10.0.0/tools/tools/vhba/simple/Makefile 208926 2010-06-08 22:26:47Z mjacob $
KMOD=	vsimple
SRCS=	vhba_simple.c vhba.c
CFLAGS += -I${.CURDIR}/.. -DVHBA_MOD=\"vsimple\"
VPATH=	${.CURDIR}/..

.include <bsd.kmod.mk>
