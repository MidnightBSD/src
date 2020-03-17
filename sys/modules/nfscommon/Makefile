# $FreeBSD: stable/11/sys/modules/nfscommon/Makefile 346463 2019-04-21 00:41:06Z rmacklem $

.PATH: ${SRCTOP}/sys/fs/nfs
KMOD=	nfscommon
SRCS=	vnode_if.h \
	nfs_commonacl.c \
	nfs_commonkrpc.c \
	nfs_commonport.c \
	nfs_commonsubs.c \
	opt_inet.h \
	opt_inet6.h \
	opt_kgssapi.h \
	opt_nfs.h \
	opt_ufs.h

.include <bsd.kmod.mk>
