# $MidnightBSD$
# $FreeBSD: stable/10/cddl/Makefile 291792 2015-12-04 18:14:31Z bdrewery $

.include <bsd.own.mk>

SUBDIR=	lib .WAIT \
	sbin usr.bin usr.sbin

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
