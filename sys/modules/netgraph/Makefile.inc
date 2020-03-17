# $FreeBSD: stable/11/sys/modules/netgraph/Makefile.inc 319182 2017-05-30 04:11:12Z ngie $
# $Whistle: Makefile.inc,v 1.4 1999/01/19 23:46:16 archie Exp $

.PATH:		${SRCTOP}/sys/netgraph

SRCS+=	opt_netgraph.h

.include "../Makefile.inc"
