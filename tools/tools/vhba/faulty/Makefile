# $FreeBSD: release/10.0.0/tools/tools/vhba/faulty/Makefile 208926 2010-06-08 22:26:47Z mjacob $
KMOD=	vfaulty
SRCS=	vhba_faulty.c vhba.c
CFLAGS += -I${.CURDIR}/.. -DVHBA_MOD=\"vfaulty\"
VPATH=	${.CURDIR}/..

.include <bsd.kmod.mk>
