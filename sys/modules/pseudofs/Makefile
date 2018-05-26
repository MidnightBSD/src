# $MidnightBSD$
# $FreeBSD: stable/10/sys/modules/pseudofs/Makefile 293595 2016-01-09 18:02:08Z dchagin $

.PATH: ${.CURDIR}/../../fs/pseudofs

KMOD=	pseudofs
SRCS=	opt_pseudofs.h \
	vnode_if.h \
	pseudofs.c \
	pseudofs_fileno.c \
	pseudofs_vncache.c \
	pseudofs_vnops.c

EXPORT_SYMS=	pfs_mount	\
		pfs_unmount	\
		pfs_root	\
		pfs_statfs	\
		pfs_init	\
		pfs_uninit	\
		pfs_create_dir	\
		pfs_create_file	\
		pfs_create_link	\
		pfs_disable	\
		pfs_enable	\
		pfs_destroy

.if !defined(KERNBUILDDIR)
.if defined(PSEUDOFS_TRACE)
CFLAGS+=-DPSEUDOFS_TRACE
.endif
.endif

.include <bsd.kmod.mk>
