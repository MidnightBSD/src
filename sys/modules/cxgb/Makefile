# $FreeBSD: stable/9/sys/modules/cxgb/Makefile 237920 2012-07-01 12:00:36Z np $
SUBDIR= cxgb
SUBDIR+= cxgb_t3fw
#SUBDIR+= ${_tom}
#SUBDIR+= ${_iw_cxgb}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
_iw_cxgb=	iw_cxgb
.endif

.include <bsd.subdir.mk>
