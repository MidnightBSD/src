#
# $FreeBSD: stable/11/sys/modules/tcp/fastpath/Makefile 319182 2017-05-30 04:11:12Z ngie $
#

.PATH: ${SRCTOP}/sys/netinet/tcp_stacks

KMOD=	fastpath
SRCS=	fastpath.c

SRCS+=	opt_inet.h opt_inet6.h opt_ipsec.h
SRCS+=	opt_tcpdebug.h

#
# Enable full debugging
#
#CFLAGS += -g

.include <bsd.kmod.mk>
