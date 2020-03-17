# $FreeBSD: stable/11/sys/modules/procfs/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/fs/procfs

KMOD=		procfs
SRCS=
SRCS+=		opt_compat.h
SRCS+=		vnode_if.h
SRCS+=		procfs_ctl.c
SRCS+=		procfs_dbregs.c
SRCS+=		procfs_fpregs.c
SRCS+=		procfs_ioctl.c
SRCS+=		procfs_map.c
SRCS+=		procfs_mem.c
SRCS+=		procfs_note.c
SRCS+=		procfs_osrel.c
SRCS+=		procfs_regs.c
SRCS+=		procfs_rlimit.c
SRCS+=		procfs_status.c
SRCS+=		procfs_type.c
SRCS+=		procfs.c

EXPORT_SYMS=
EXPORT_SYMS+=	procfs_attr
EXPORT_SYMS+=	procfs_candebug
EXPORT_SYMS+=	procfs_docurproc
EXPORT_SYMS+=	procfs_doprocfile
EXPORT_SYMS+=	procfs_doprocmem
EXPORT_SYMS+=	procfs_notsystem

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_43 1" > ${.TARGET}
	echo "#define COMPAT_FREEBSD4 1" >> ${.TARGET}
	echo "#define COMPAT_FREEBSD5 1" >> ${.TARGET}
	echo "#define COMPAT_FREEBSD6 1" >> ${.TARGET}
.if ${MACHINE_CPUARCH} == "amd64"
	echo "#define COMPAT_FREEBSD32 1" >> ${.TARGET}
	echo "#define COMPAT_LINUX32 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
