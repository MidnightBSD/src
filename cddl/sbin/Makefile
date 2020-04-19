# $FreeBSD: stable/11/cddl/sbin/Makefile 283147 2015-05-20 12:26:09Z ngie $

.include <src.opts.mk>

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
