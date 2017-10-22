# $FreeBSD: release/10.0.0/sys/modules/coff/Makefile 89260 2002-01-11 15:49:02Z ru $

.PATH: ${.CURDIR}/../../i386/ibcs2

KMOD=	ibcs2_coff
SRCS= 	imgact_coff.c vnode_if.h

CFLAGS+=	-DCOMPAT_IBCS2

.include <bsd.kmod.mk>
