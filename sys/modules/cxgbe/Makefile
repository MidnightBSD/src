#
# $FreeBSD: release/9.2.0/sys/modules/cxgbe/Makefile 252814 2013-07-05 18:27:38Z np $
#

SUBDIR = if_cxgbe
SUBDIR+= t4_firmware
SUBDIR+= t5_firmware
SUBDIR+= ${_tom}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
.endif

.include <bsd.subdir.mk>
