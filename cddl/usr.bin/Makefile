# $MidnightBSD$
# $FreeBSD: stable/10/cddl/usr.bin/Makefile 289813 2015-10-23 08:35:17Z avg $

.include <bsd.own.mk>

SUBDIR= \
	ctfconvert \
	ctfdump \
	ctfmerge \
	${_tests} \
	${_zinject} \
	${_zlook} \
	${_zstreamdump} \
	${_ztest}

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

.if ${MK_ZFS} != "no"
_zinject= zinject
#_zlook= zlook
.if ${MK_LIBTHR} != "no"
_ztest=	ztest
_zstreamdump = zstreamdump
.endif
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
