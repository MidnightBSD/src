# $FreeBSD: release/10.0.0/sys/modules/ipwfw/ipw_ibss/Makefile 167165 2007-03-02 11:42:56Z flz $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_ibss
FIRMWS=	ipw_ibss:ipw_ibss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_ibss

ipw_ibss: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3-i.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

