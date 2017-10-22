# $FreeBSD: release/10.0.0/sys/modules/mac_bsdextended/Makefile 184331 2008-10-27 12:09:15Z rwatson $

.PATH: ${.CURDIR}/../../security/mac_bsdextended

KMOD=	mac_bsdextended
SRCS=	vnode_if.h \
	mac_bsdextended.h \
	mac_bsdextended.c \
	ugidfw_system.c \
	ugidfw_vnode.c

.include <bsd.kmod.mk>
