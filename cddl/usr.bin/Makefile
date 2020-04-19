# $FreeBSD: stable/11/cddl/usr.bin/Makefile 288339 2015-09-28 12:38:57Z avg $

.include <src.opts.mk>

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
