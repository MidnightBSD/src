# $FreeBSD: src/sys/modules/coda5/Makefile,v 1.4.2.2 2006/02/22 11:55:26 yar Exp $

.PATH: ${.CURDIR}/../../coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CFLAGS+= -DCODA_COMPAT_5

.include <bsd.kmod.mk>
