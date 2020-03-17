# $FreeBSD: stable/11/sys/modules/urtwnfw/Makefile.inc 297058 2016-03-20 03:54:57Z adrian $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}:111

# FIRMWARE_LICENSE=	realtek

${_FIRM}: ${.CURDIR}/../../../contrib/dev/urtwn/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
