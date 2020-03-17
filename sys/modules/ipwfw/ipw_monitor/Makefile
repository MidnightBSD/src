# $FreeBSD: stable/11/sys/modules/ipwfw/ipw_monitor/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/ipw

KMOD=	ipw_monitor
FIRMWS=	ipw_monitor:ipw_monitor:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_monitor

ipw_monitor: ipw2100-1.3-p.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>

