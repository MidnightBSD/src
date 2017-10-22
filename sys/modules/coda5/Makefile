# $FreeBSD: stable/9/sys/modules/coda5/Makefile 176241 2008-02-13 16:31:04Z rwatson $

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CFLAGS+= -DCODA5_MODULE

.include <bsd.kmod.mk>
