# $FreeBSD: stable/11/sys/modules/arcnet/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/net

KMOD=	arcnet
SRCS=	if_arcsubr.c
SRCS+=	opt_inet.h opt_inet6.h opt_ofed.h

EXPORT_SYMS=	arc_frag_init	\
		arc_frag_next	\
		arc_ifattach	\
		arc_ifdetach	\
		arc_input	\
		arc_ioctl	\
		arc_isphds	\
		arc_output	\
		arc_storelladdr

.include <bsd.kmod.mk>
