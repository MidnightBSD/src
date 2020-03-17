# Cronyx Id: sys.modules.cx.Makefile,v 1.1.2.2 2003/01/21 15:15:49 rik Exp $
# $FreeBSD: stable/11/sys/modules/cx/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/dev/cx
KMOD=   if_cx
SRCS=   if_cx.c cxddk.c csigma.c opt_netgraph.h opt_ng_cronyx.h \
	device_if.h bus_if.h isa_if.h

.if !defined(KERNBUILDDIR)
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH $(NETGRAPH)" > ${.TARGET}
.endif

.if ${NG_CRONYX} != 0
opt_ng_cronyx.h:
	echo "#define NETGRAPH_CRONYX 1" > ${.TARGET}
.endif
.endif	

.include <bsd.kmod.mk>
