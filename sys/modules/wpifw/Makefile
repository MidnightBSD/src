# $FreeBSD: release/7.0.0/sys/modules/wpifw/Makefile 174446 2007-12-08 12:44:59Z benjsc $

.PATH: ${.CURDIR}/../../contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-2.14.4.fw:wpifw:2144
FIRMWARE_LICENSE= intel_wpi

CLEANFILES=	iwlwifi-3945-2.14.4.fw

iwlwifi-3945-2.14.4.fw: ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-2.14.4.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
