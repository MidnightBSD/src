# $FreeBSD: release/10.0.0/sys/modules/netgraph/netflow/Makefile 232921 2012-03-13 11:08:40Z melifaro $
#
# Author: Gleb Smirnoff <glebius@freebsd.org>
#

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../../netgraph/netflow

KMOD=   ng_netflow
SRCS=   ng_netflow.c netflow.c netflow_v9.c opt_inet6.h opt_route.h

.if !defined(KERNBUILDDIR)

.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif

.endif

.include <bsd.kmod.mk>
