# $FreeBSD: release/10.0.0/sys/modules/ipwfw/ipw_bss/Makefile 167165 2007-03-02 11:42:56Z flz $

.PATH: ${.CURDIR}/../../../contrib/dev/ipw

KMOD=	ipw_bss
FIRMWS=	ipw_bss:ipw_bss:130
FIRMWARE_LICENSE=	intel_ipw

CLEANFILES=	ipw_bss

ipw_bss: ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3.fw.uu
	uudecode -p ${.CURDIR}/../../../contrib/dev/ipw/ipw2100-1.3.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>

