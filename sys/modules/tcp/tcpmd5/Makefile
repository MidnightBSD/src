# $FreeBSD: stable/11/sys/modules/tcp/tcpmd5/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/netipsec

KMOD=   tcpmd5
SRCS=   xform_tcp.c opt_inet.h opt_inet6.h opt_ipsec.h

opt_ipsec.h:
	@echo "#define IPSEC_SUPPORT 1" > ${.TARGET}

.include <bsd.kmod.mk>
