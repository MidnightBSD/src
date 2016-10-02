# $MidnightBSD$
#
# $FreeBSD: stable/9/sys/modules/cxgbe/Makefile 237920 2012-07-01 12:00:36Z np $
#

SUBDIR = if_cxgbe
SUBDIR+= firmware
#SUBDIR+= ${_tom}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
.endif

.include <bsd.subdir.mk>
