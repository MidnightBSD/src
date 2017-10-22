# $FreeBSD: release/10.0.0/sys/modules/siftr/Makefile 209662 2010-07-03 13:32:39Z lstewart $

.include <bsd.own.mk>

.PATH:  ${.CURDIR}/../../netinet
KMOD=	siftr
SRCS=	siftr.c

# Uncomment to add IPv6 support
#CFLAGS+=-DSIFTR_IPV6

.include <bsd.kmod.mk>
