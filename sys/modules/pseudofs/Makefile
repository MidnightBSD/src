# $FreeBSD: stable/11/sys/modules/pseudofs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/pseudofs

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
