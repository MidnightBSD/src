# $FreeBSD: stable/11/usr.sbin/bsnmpd/modules/snmp_netgraph/Makefile 275054 2014-11-25 16:57:27Z bapt $
#
# Author: Harti Brandt <harti@freebsd.org>

MOD=	netgraph
SRCS=	snmp_netgraph.c
XSYM=	begemotNg
MAN=	snmp_netgraph.3

BMIBS=	BEGEMOT-NETGRAPH.txt
DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h

LIBADD=	netgraph

.include <bsd.snmpmod.mk>
