# $FreeBSD: stable/11/sys/modules/wpifw/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/contrib/dev/wpi

KMOD=	wpifw
FIRMWS=	iwlwifi-3945-15.32.2.9.fw:wpifw:153229

CLEANFILES=	iwlwifi-3945-15.32.2.9.fw

iwlwifi-3945-15.32.2.9.fw: iwlwifi-3945-15.32.2.9.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
