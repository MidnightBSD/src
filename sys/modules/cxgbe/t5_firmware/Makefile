#
#

T5FW=	${SRCTOP}/sys/dev/cxgbe/firmware
.PATH: ${T5FW}

KMOD=	t5fw_cfg
FIRMWS=	${KMOD}.txt:${KMOD}:1.0.0.0

# You can have additional configuration files in the ${T5FW} directory.
# t5fw_cfg_<name>.txt
CFG_FILES != cd ${T5FW} && echo ${KMOD}_*.txt
.for F in ${CFG_FILES}
.if exists(${F})
FIRMWS+=	${F}:${F:C/.txt//}:1.0.0.0
.endif
.endfor

T5FW_VER=	1.25.0.40
FIRMWS+=	t5fw-${T5FW_VER}.bin:t5fw:${T5FW_VER}

.include <bsd.kmod.mk>
