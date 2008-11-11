# $MidnightBSD: src/gnu/lib/Makefile,v 1.2 2007/07/26 20:27:46 laffer1 Exp $
# $FreeBSD: src/gnu/lib/Makefile,v 1.40 2004/08/14 03:23:24 kan Exp $

.include <bsd.own.mk>

SUBDIR= csu libgcc libgcov libdialog libregex libreadline

# libsupc++ uses libstdc++ headers, although 'make includes' should
# have taken care of that already.
.if ${MK_CXX} != "no"
SUBDIR+= libstdc++ libsupc++
.endif

.if ${MK_OBJC} != "no"
SUBDIR+= libobjc
.endif

.if !defined(NO_FORTRAN)
SUBDIR+= libg2c
.endif

.include <bsd.subdir.mk>
