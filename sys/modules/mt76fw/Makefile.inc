#
# Common rules for building MediaTek mt76 firmware.  Note this gets
# auto-included by the subdir Makefiles as a consequence of included
# bsd.kmod.mk.
#
# The drivers request "mediatek/[<chip>/]<file>"; LinuxKPI falls back to
# the file name without the path, which is what the modules register.

IMG=		${NAME}
KMOD=		${NAME}

CLEANFILES+=	${IMG}

FIRMWS=		${IMG}:${IMG}:${VERSION}

# Redistributable under LICENCE.mediatek; no license ack needed.
#FIRMWARE_LICENSE=

${IMG}: ${SRCTOP}/sys/contrib/dev/mt76fw/${IMG}
	cp ${.ALLSRC} ${.TARGET}
