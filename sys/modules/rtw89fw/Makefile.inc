#
# Common rules for building firmware.  Note this gets auto-included
# by the subdir Makefile's as a consequence of included bsd.kmod.mk.

# FWSFX is the "-N" firmware format suffix, empty for the plain image.
FWSFX?=
_NAME=		rtw${NAME}_fw${FWSFX}.bin

IMG=		${_NAME}
KMOD=		${_NAME}

CLEANFILES+=	${IMG}

FIRMWS=		${IMG}:${IMG}:${VERSION}

${IMG}: ${SRCTOP}/sys/contrib/dev/rtw89fw/${IMG}
	cp ${.ALLSRC} ${.TARGET}

