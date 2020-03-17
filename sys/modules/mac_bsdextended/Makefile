# $FreeBSD: stable/11/sys/modules/mac_bsdextended/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/security/mac_bsdextended

KMOD=	mac_bsdextended
SRCS=	vnode_if.h \
	mac_bsdextended.h \
	mac_bsdextended.c \
	ugidfw_system.c \
	ugidfw_vnode.c

.include <bsd.kmod.mk>
