# $FreeBSD: stable/11/sys/modules/if_gif/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/net ${SYSDIR}/netinet ${SYSDIR}/netinet6

KMOD=	if_gif
SRCS=	if_gif.c opt_inet.h opt_inet6.h
SRCS.INET=in_gif.c
SRCS.INET6=in6_gif.c

.include <bsd.kmod.mk>
