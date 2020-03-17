# $FreeBSD: stable/11/sys/modules/netgraph/checksum/Makefile 303612 2016-08-01 12:09:04Z julian $

.include <bsd.own.mk>

KMOD=	ng_checksum
SRCS= 	ng_checksum.c opt_inet.h opt_inet6.h

#.if !defined(KERNBUILDDIR)
#
#.if ${MK_INET_SUPPORT} != "no"
#opt_inet.h:
#	echo "#define INET 1" > ${.TARGET}
#.endif
#.if ${MK_INET6_SUPPORT} != "no"
#opt_inet6.h:
#	echo "#define INET6 1" > ${.TARGET}
#.endif
#.endif

.include <bsd.kmod.mk>
