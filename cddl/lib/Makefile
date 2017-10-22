# $FreeBSD: release/7.0.0/cddl/lib/Makefile 172500 2007-10-09 23:31:11Z obrien $

.include <bsd.own.mk>

SUBDIR=	libavl \
	libnvpair \
	libumem \
	libuutil \
	${_libzfs} \
	${_libzpool}

.if ${MK_ZFS} != "no"
_libzfs=	libzfs
.if ${MK_LIBTHR} != "no" || ${MK_LIBKSE} != "no"
_libzpool=	libzpool
.endif
.endif

.include <bsd.subdir.mk>
