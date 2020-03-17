# $FreeBSD: stable/11/sys/modules/carp/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netinet
.PATH: ${SRCTOP}/sys/crypto

KMOD=	carp
SRCS=	ip_carp.c sha1.c
SRCS+=	device_if.h bus_if.h vnode_if.h
SRCS+=	opt_carp.h opt_bpf.h opt_inet.h opt_inet6.h opt_ofed.h

.include <bsd.kmod.mk>
