# $MidnightBSD$
# $FreeBSD: stable/10/gnu/lib/Makefile 291792 2015-12-04 18:14:31Z bdrewery $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libregex libreadline

.if ${MK_SSP} != "no"
SUBDIR+= libssp
.endif

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_GNUCXX} != "no"
SUBDIR+= libstdc++ libsupc++
SUBDIR_DEPENDS_libsupc++:= libstdc++
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
