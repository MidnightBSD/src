# $FreeBSD: release/10.0.0/sys/modules/cxgb/Makefile 237263 2012-06-19 07:34:13Z np $
SUBDIR= cxgb
SUBDIR+= cxgb_t3fw
SUBDIR+= ${_tom}
SUBDIR+= ${_iw_cxgb}

.if ${MACHINE_CPUARCH} == "amd64" || ${MACHINE_CPUARCH} == "i386"
_tom=		tom
_iw_cxgb=	iw_cxgb
.endif

.include <bsd.subdir.mk>
