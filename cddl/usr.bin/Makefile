# $FreeBSD: release/7.0.0/cddl/usr.bin/Makefile 172500 2007-10-09 23:31:11Z obrien $

.include <bsd.own.mk>

SUBDIR=	${_ztest}

.if ${MK_ZFS} != "no"
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_ztest=	ztest
.endif
.endif

.include <bsd.subdir.mk>
