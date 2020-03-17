# $FreeBSD: stable/11/sys/modules/drm2/radeonkmsfw/Makefile.inc 319182 2017-05-30 04:11:12Z ngie $
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

${_FIRM}: ${SRCTOP}/sys/contrib/dev/drm2/radeonkmsfw/${_FIRM}.uu
	uudecode -p $? > ${.TARGET}
