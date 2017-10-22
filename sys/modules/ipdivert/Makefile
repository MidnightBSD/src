# $FreeBSD: release/10.0.0/sys/modules/ipdivert/Makefile 231838 2012-02-16 21:57:50Z uqs $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../../netinet

KMOD=   ipdivert
SRCS=   ip_divert.c opt_inet.h opt_inet6.h opt_sctp.h

.if !defined(KERNBUILDDIR)
.if ${MK_INET_SUPPORT} != "no"
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif
.if ${MK_INET6_SUPPORT} != "no"
opt_inet6.h:
	echo "#define INET6 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
