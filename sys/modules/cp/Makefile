# $Cronyx: sys.modules.cp.Makefile,v 1.1.2.3 2004/04/28 16:17:57 rik Exp $
# $FreeBSD: stable/11/sys/modules/cp/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH:  ${SRCTOP}/sys/dev/cp
KMOD	= if_cp
SRCS	= if_cp.c cpddk.c
SRCS   += opt_netgraph.h opt_ng_cronyx.h device_if.h bus_if.h \
	  pci_if.h

.if !defined(KERNBUILDDIR)
PROTOS?=	-DINET
NG_CRONYX?=	0
NETGRAPH=	${NG_CRONYX}

CFLAGS+= ${PROTOS}

.if ${NETGRAPH} != 0
opt_netgraph.h:
	echo "#define NETGRAPH ${NETGRAPH}" > ${.TARGET}
.endif

.if ${NG_CRONYX} != 0
opt_ng_cronyx.h:
	echo "#define NETGRAPH_CRONYX 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>

CWARNFLAGS.cpddk.c+=	${NO_WMISLEADING_INDENTATION}
