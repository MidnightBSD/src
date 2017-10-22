# $FreeBSD: release/10.0.0/tools/tools/vhba/rptluns/Makefile 208926 2010-06-08 22:26:47Z mjacob $
KMOD=	vrptluns
SRCS=	vhba_rptluns.c vhba.c
CFLAGS += -I${.CURDIR}/.. -DVHBA_MOD=\"vrptluns\"
VPATH=	${.CURDIR}/..

.include <bsd.kmod.mk>
