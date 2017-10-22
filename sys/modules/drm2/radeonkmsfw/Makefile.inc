# $FreeBSD: release/10.0.0/sys/modules/drm2/radeonkmsfw/Makefile.inc 254885 2013-08-25 19:37:15Z dumbbell $
#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

_FIRM=	${IMG}.bin

CLEANFILES+=	${_FIRM}

FIRMWS=	${_FIRM}:${KMOD}

#
# Note that a license ack is not needed for iwn.
#
#FIRMWARE_LICENSE=

${_FIRM}: ${.CURDIR}/../../../../contrib/dev/drm2/radeonkmsfw/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
