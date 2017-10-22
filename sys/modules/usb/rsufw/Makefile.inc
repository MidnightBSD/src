# $FreeBSD: release/10.0.0/sys/modules/usb/rsufw/Makefile.inc 253761 2013-07-29 06:57:45Z rpaulo $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}:120

FIRMWARE_LICENSE=	realtek

${_FIRM}: ${.CURDIR}/../../../../contrib/dev/rsu/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
