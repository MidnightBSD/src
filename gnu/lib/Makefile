# $MidnightBSD$
# $FreeBSD: src/gnu/lib/Makefile,v 1.40 2004/08/14 03:23:24 kan Exp $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libgomp libodialog libregex libreadline \
	libssp

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.include <bsd.subdir.mk>
