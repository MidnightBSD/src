# $FreeBSD: stable/11/sys/modules/iwifw/iwi_ibss/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/iwi

KMOD=	iwi_ibss
FIRMWS=	iwi_ibss:iwi_ibss:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_fw

iwi_ibss: ipw2200-ibss.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>

