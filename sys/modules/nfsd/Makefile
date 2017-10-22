# $FreeBSD: stable/9/sys/modules/nfsd/Makefile 192991 2009-05-28 19:45:11Z rmacklem $

.PATH: ${.CURDIR}/../../fs/nfsserver
KMOD=	nfsd
SRCS=	vnode_if.h \
	nfs_nfsdserv.c \
	nfs_nfsdcache.c \
	nfs_nfsdkrpc.c \
	nfs_nfsdsocket.c \
	nfs_nfsdstate.c \
	nfs_nfsdsubs.c \
	nfs_nfsdport.c \
	opt_ufs.h \
	opt_nfs.h \
	opt_inet6.h \
	opt_kgssapi.h

.include <bsd.kmod.mk>
