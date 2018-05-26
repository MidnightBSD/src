# $MidnightBSD$
# $Whistle: Makefile,v 1.1 1999/12/08 20:20:39 archie Exp $
# $FreeBSD: stable/10/sys/modules/netgraph/mppc/Makefile 302061 2016-06-21 15:47:54Z pfg $

KMOD=	ng_mppc
SRCS= 	ng_mppc.c opt_netgraph.h

NETGRAPH_MPPC_COMPRESSION?=	1
NETGRAPH_MPPC_ENCRYPTION?=	1

.if ${NETGRAPH_MPPC_COMPRESSION} > 0
.PATH: ${.CURDIR}/../../../net
SRCS+=	mppcc.c mppcd.c
.endif

.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
.PATH: ${.CURDIR}/../../../crypto
SRCS+= sha1.c
.endif

.if !defined(KERNBUILDDIR)
opt_netgraph.h:
	:> ${.TARGET}
.if ${NETGRAPH_MPPC_COMPRESSION} > 0
	echo "#define NETGRAPH_MPPC_COMPRESSION 1" >> ${.TARGET}
.endif
.if ${NETGRAPH_MPPC_ENCRYPTION} > 0
	echo "#define NETGRAPH_MPPC_ENCRYPTION 1" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
