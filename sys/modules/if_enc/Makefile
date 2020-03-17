# $FreeBSD: stable/11/sys/modules/if_enc/Makefile 319182 2017-05-30 04:11:12Z ngie $

SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

.PATH: ${SYSDIR}/net

KMOD=	if_enc
SRCS=	if_enc.c opt_inet.h opt_inet6.h

.include <bsd.kmod.mk>
