# $FreeBSD: release/7.0.0/sys/modules/coda/Makefile 171416 2007-07-12 21:04:58Z rwatson $

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

.include <bsd.kmod.mk>
