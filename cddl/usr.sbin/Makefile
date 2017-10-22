# $FreeBSD: release/7.0.0/cddl/usr.sbin/Makefile 172500 2007-10-09 23:31:11Z obrien $

.include <bsd.own.mk>

SUBDIR=	${_zdb}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_zdb=	zdb
.endif
.endif

.include <bsd.subdir.mk>
