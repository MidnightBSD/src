# $FreeBSD: stable/9/gnu/lib/Makefile 220755 2011-04-17 21:03:23Z dim $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libodialog libregex libreadline \
	libssp

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.include <bsd.subdir.mk>
