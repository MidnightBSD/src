# $FreeBSD: stable/11/sys/modules/sppp/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/net

KMOD=	sppp
SRCS=	if_spppfr.c if_spppsubr.c slcompress.c
SRCS+=	opt_inet.h opt_inet6.h

EXPORT_SYMS=	sppp_attach	\
		sppp_detach	\
		sppp_input	\
		sppp_ioctl	\
		sppp_dequeue	\
		sppp_pick	\
		sppp_isempty	\
		sppp_flush

.include <bsd.kmod.mk>
