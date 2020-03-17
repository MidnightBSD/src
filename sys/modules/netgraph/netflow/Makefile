# $FreeBSD: stable/11/sys/modules/netgraph/netflow/Makefile 319182 2017-05-30 04:11:12Z ngie $
#
# Author: Gleb Smirnoff <glebius@freebsd.org>
#

.PATH: ${SRCTOP}/sys/netgraph/netflow

KMOD=   ng_netflow
SRCS=   ng_netflow.c netflow.c netflow_v9.c opt_inet6.h opt_route.h

.include <bsd.kmod.mk>
