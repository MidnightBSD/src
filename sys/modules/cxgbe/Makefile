# $MidnightBSD$
#
# $FreeBSD: stable/10/sys/modules/cxgbe/Makefile 309560 2016-12-05 20:43:25Z jhb $
#

SUBDIR=	if_cxgbe
SUBDIR+=	if_cxl
SUBDIR+=	if_cc
SUBDIR+=	if_cxgbev
SUBDIR+=	if_cxlv
SUBDIR+=	if_ccv
SUBDIR+=	t4_firmware
SUBDIR+=	t5_firmware
SUBDIR+=	t6_firmware
SUBDIR+=	${_tom}
SUBDIR+=	${_iw_cxgbe}

.if ${MACHINE_CPUARCH} == "amd64"
_tom=	tom
_iw_cxgbe=	iw_cxgbe
.endif

.if ${MACHINE_CPUARCH} == "i386"
_tom=	tom
.endif


.include <bsd.subdir.mk>
