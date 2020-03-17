# $FreeBSD: stable/11/sys/modules/usb/runfw/Makefile 351876 2019-09-05 17:20:24Z bdrewery $

KMOD=	runfw
FIRMWS=	run.fw:runfw:1

CLEANFILES=	run.fw

run.fw: ${SRCTOP}/sys/contrib/dev/run/rt2870.fw.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}

.include <bsd.kmod.mk>
