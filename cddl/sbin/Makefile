# $MidnightBSD$
# $FreeBSD: stable/10/cddl/sbin/Makefile 283853 2015-05-31 23:00:35Z ngie $

.include <bsd.own.mk>

SUBDIR=	${_tests} ${_zfs} ${_zpool}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.if ${MK_ZFS} != "no"
_zfs=	zfs
_zpool=	zpool
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
