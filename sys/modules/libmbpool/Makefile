# $FreeBSD: stable/11/sys/modules/libmbpool/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/kern

KMOD=	libmbpool
SRCS=	subr_mbpool.c

EXPORT_SYMS=	mbp_create	\
		mbp_destroy	\
		mbp_alloc	\
		mbp_free	\
		mbp_ext_free	\
		mbp_card_free	\
		mbp_count	\
		mbp_get		\
		mbp_get_keep	\
		mbp_sync

.include <bsd.kmod.mk>
