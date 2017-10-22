# $FreeBSD: stable/9/sys/modules/runfw/Makefile 203148 2010-01-29 03:35:01Z thompsa $

KMOD=	runfw
FIRMWS=	runfw:runfw:1

CLEANFILES=	runfw

runfw: ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu
	uudecode -p ${.CURDIR}/../../contrib/dev/run/rt2870.fw.uu > ${.TARGET}

.include <bsd.kmod.mk>
