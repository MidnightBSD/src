# $FreeBSD: stable/11/sys/modules/nfslockd/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/nlm ${SRCTOP}/sys/rpc
KMOD=	nfslockd
SRCS=	vnode_if.h \
	nlm_advlock.c \
	nlm_prot_clnt.c \
	nlm_prot_impl.c \
	nlm_prot_server.c \
	nlm_prot_svc.c \
	nlm_prot_xdr.c \
	sm_inter_xdr.c
SRCS+=	opt_inet6.h opt_nfs.h

.include <bsd.kmod.mk>
