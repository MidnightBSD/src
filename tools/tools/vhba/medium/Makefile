# $FreeBSD: release/10.0.0/tools/tools/vhba/medium/Makefile 208926 2010-06-08 22:26:47Z mjacob $
KMOD=	vmedium
SRCS=	vhba_medium.c vhba.c
CFLAGS += -I${.CURDIR}/.. -DVHBA_MOD=\"vmedium\"
VPATH=	${.CURDIR}/..

.include <bsd.kmod.mk>
