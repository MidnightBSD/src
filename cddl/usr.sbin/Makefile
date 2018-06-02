# $MidnightBSD$
# $FreeBSD: stable/10/cddl/usr.sbin/Makefile 283853 2015-05-31 23:00:35Z ngie $

.include <bsd.own.mk>

SUBDIR=	${_dtrace} \
	${_dtruss} \
	${_lockstat} \
	${_tests} \
	${_zdb} \
	${_zhack}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

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

.if ${MACHINE_CPUARCH} == "mips"
_dtrace=	dtrace
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
_dtrace=	dtrace
_dtruss=	dtruss
_lockstat=	lockstat
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
