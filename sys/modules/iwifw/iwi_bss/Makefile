# $FreeBSD: stable/11/sys/modules/iwifw/iwi_bss/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/iwi

KMOD=	iwi_bss
FIRMWS=	iwi_bss:iwi_bss:300
FIRMWARE_LICENSE=	intel_iwi

CLEANFILES=	iwi_bss

iwi_bss: ipw2200-bss.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>

