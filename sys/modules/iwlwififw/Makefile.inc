
# Common rules for building iwlwifi firmware.

EXT?=		ucode
# FWFILE is the version as spelled in the file name ("c101" for
# core-numbered images); FWVERSION must stay numeric for firmware(9).
FWFILE?=	${FWVERSION}
_NAME?=		iwlwifi-${NAME}-${FWFILE}.${EXT}

IMG=		${_NAME}
KMOD=		${_NAME}

CLEANFILES+=	${IMG}

FIRMWS=		${IMG}:${IMG}:${FWVERSION}

# License ack is not needed for iwlwifi (same as iwn/iwm).
#FIRMWARE_LICENSE=

${IMG}: ${SRCTOP}/sys/contrib/dev/iwlwififw/${IMG}
	cp ${.ALLSRC} ${.TARGET}

