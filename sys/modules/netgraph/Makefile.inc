# $FreeBSD: release/10.0.0/sys/modules/netgraph/Makefile.inc 146259 2005-05-16 08:25:55Z glebius $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${.CURDIR}/../../../netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
