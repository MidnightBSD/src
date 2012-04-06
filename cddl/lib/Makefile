# $MidnightBSD: src/cddl/lib/Makefile,v 1.2 2008/10/15 19:10:19 laffer1 Exp $

.include <bsd.own.mk>

SUBDIR=	drti \
	libavl \
	libctf \
	libdtrace \
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
