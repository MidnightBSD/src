# $FreeBSD: stable/9/cddl/lib/Makefile 211550 2010-08-21 11:14:50Z rpaulo $

.include <bsd.own.mk>

SUBDIR=	${_drti} \
	libavl \
	libctf \
	${_libdtrace} \
	libnvpair \
	libumem \
	libuutil \
	${_libzfs} \
	${_libzpool}

.if ${MK_ZFS} != "no"
_libzfs=	libzfs
.if ${MK_LIBTHR} != "no"
_libzpool=	libzpool
.endif
.endif

.if ${MACHINE_ARCH} == "amd64" || ${MACHINE_ARCH} == "i386"
_drti=		drti
_libdtrace=	libdtrace
.endif

.include <bsd.subdir.mk>
