# $FreeBSD: release/10.0.0/sys/modules/iwifw/iwi_monitor/Makefile 167165 2007-03-02 11:42:56Z flz $

.PATH: ${.CURDIR}/../../../contrib/dev/iwi

KMOD=	iwi_monitor
FIRMWS=	iwi_monitor:iwi_monitor:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_monitor

iwi_monitor: ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-sniffer.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/iwi/ipw2200-sniffer.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

