# $RISS: if_arl/modules/if_arl/Makefile,v 1.3 2003/01/13 08:05:29 frol Exp $
# $FreeBSD: release/7.0.0/sys/modules/arl/Makefile 151350 2005-10-14 23:30:17Z yar $

.PATH:	${.CURDIR}/../../dev/arl

KMOD=	if_arl
SRCS=	if_arl.c if_arl_isa.c
SRCS+=	opt_inet.h device_if.h bus_if.h isa_if.h
CFLAGS+= -I${.CURDIR}/../..

.if !defined(KERNBUILDDIR)
opt_inet.h:
	echo "#define INET 1" > opt_inet.h
.endif

.include <bsd.kmod.mk>
