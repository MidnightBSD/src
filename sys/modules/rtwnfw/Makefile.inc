# $FreeBSD: stable/11/sys/modules/rtwnfw/Makefile.inc 351876 2019-09-05 17:20:24Z bdrewery $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.fw

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}:111

FIRMWARE_LICENSE=	realtek

${_FIRM}: ${SRCTOP}/sys/contrib/dev/rtwn/${_FIRM}.uu
	uudecode -p ${.ALLSRC} > ${.TARGET}
