# $FreeBSD: release/7.0.0/gnu/lib/Makefile 169718 2007-05-19 04:25:59Z kan $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libregex libreadline

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.if ${MK_SSP} != "no"
SUBDIR+= libssp
.endif

.include <bsd.subdir.mk>
