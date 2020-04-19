# $FreeBSD: stable/11/cddl/Makefile 289393 2015-10-15 22:55:08Z bdrewery $

.include <src.opts.mk>

SUBDIR=	lib .WAIT \
	sbin usr.bin usr.sbin

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
