#	@(#)Makefile	8.1 (Berkeley) 6/12/93
# $FreeBSD: src/usr.bin/netstat/Makefile,v 1.26.2.3 2006/01/05 03:47:24 kbyanc Exp $
# $MidnightBSD: src/usr.bin/netstat/Makefile,v 1.5 2007/03/21 00:26:24 laffer1 Exp $

PROG=	netstat
SRCS=	bpf.c if.c inet.c main.c mbuf.c mcast.c mroute.c ipx.c route.c \
	unix.c atalk.c netgraph.c mroute6.c ipsec.c pfkey.c

WARNS?=	3

CFLAGS+=-DIPSEC
.if !defined(NO_INET6)
CFLAGS+=-DINET6
CFLAGS+=-DFAST_IPSEC
SRCS+=	inet6.c
.endif

BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBIPX} ${LIBMEMSTAT} ${LIBNETGRAPH} ${LIBUTIL}
LDADD=	-lkvm -lipx -lmemstat -lnetgraph -lutil

.include <bsd.prog.mk>
