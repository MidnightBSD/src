# $FreeBSD: stable/11/sys/modules/bktr/bktr_mem/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/bktr

KMOD=	bktr_mem 
SRCS=	bktr_mem.c
EXPORT_SYMS=	bktr_has_stored_addresses	\
		bktr_store_address		\
		bktr_retrieve_address

.include <bsd.kmod.mk>
