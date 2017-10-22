# $FreeBSD: stable/9/cddl/usr.sbin/Makefile 243674 2012-11-29 14:05:04Z mm $

.include <bsd.own.mk>

SUBDIR=	${_dtrace} \
	${_dtruss} \
	${_lockstat} \
	${_zdb} \
	${_zhack}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no"
_zdb=	zdb
_zhack=	zhack
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

.include <bsd.subdir.mk>
