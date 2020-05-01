#	@(#)Makefile	5.33.1.1 (Berkeley) 5/6/91
# $FreeBSD: stable/11/gnu/Makefile 289393 2015-10-15 22:55:08Z bdrewery $

.include <src.opts.mk>

SUBDIR= lib .WAIT \
	${_tests} usr.bin

.if ${MK_TESTS} != "no"
_tests=	tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
