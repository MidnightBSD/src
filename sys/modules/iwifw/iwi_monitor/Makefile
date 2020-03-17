# $FreeBSD: stable/11/sys/modules/iwifw/iwi_monitor/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/iwi

KMOD=	iwi_monitor
FIRMWS=	iwi_monitor:iwi_monitor:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_monitor

iwi_monitor: ipw2200-sniffer.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
