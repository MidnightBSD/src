# $FreeBSD: release/10.0.0/sys/modules/wpifw/Makefile 216559 2010-12-19 11:37:44Z bschmidt $

.PATH: ${.CURDIR}/../../contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-15.32.2.9.fw:wpifw:153229

CLEANFILES=	iwlwifi-3945-15.32.2.9.fw

iwlwifi-3945-15.32.2.9.fw: ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-15.32.2.9.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/wpi/iwlwifi-3945-15.32.2.9.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
