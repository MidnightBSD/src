# $FreeBSD: stable/11/sys/modules/if_gre/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.PATH: ${SYSDIR}/net ${SYSDIR}/netinet ${SYSDIR}/netinet6
.include "${SYSDIR}/conf/kern.opts.mk"

KMOD=	if_gre
SRCS=	if_gre.c opt_inet.h opt_inet6.h
SRCS.INET=	ip_gre.c
SRCS.INET6=	ip6_gre.c

.include <bsd.kmod.mk>
