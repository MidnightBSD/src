# $FreeBSD: release/10.0.0/sys/modules/usb/runfw/Makefile 253795 2013-07-30 08:09:48Z pluknet $

KMOD=	runfw
FIRMWS=	run.fw:runfw:1

CLEANFILES=	run.fw

run.fw: ${.CURDIR}/../../../contrib/dev/run/rt2870.fw.uu
	uudecode -p $? > ${.TARGET}

.include <bsd.kmod.mk>
