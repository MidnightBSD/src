# $FreeBSD: stable/11/sys/modules/ipwfw/ipw_bss/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/ipw

KMOD=	ipw_bss
FIRMWS=	ipw_bss:ipw_bss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_bss

ipw_bss: ipw2100-1.3.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>

