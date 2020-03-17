# $FreeBSD: stable/11/sys/modules/ipwfw/ipw_ibss/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/ipw

KMOD=	ipw_ibss
FIRMWS=	ipw_ibss:ipw_ibss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_ibss

ipw_ibss: ipw2100-1.3-i.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>

