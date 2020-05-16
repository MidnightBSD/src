# $FreeBSD: stable/11/usr.bin/ofed/Makefile 337682 2018-08-12 08:45:23Z kib $

.include <src.opts.mk>

SUBDIR= \
	libibverbs \
	librdmacm \
	infiniband-diags

SUBDIR.${MK_OFED_EXTRA}+= opensm

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
