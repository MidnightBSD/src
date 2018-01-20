# $MidnightBSD$
# $FreeBSD: src/gnu/usr.bin/Makefile,v 1.84 2005/03/21 05:30:58 kientzle Exp $

.include <bsd.own.mk>

SUBDIR= ${_binutils} \
	${_cc} \
	${_cvs} \
	dialog \
	diff \
	${_dtc} \
	${_gdb} \
	${_gperf} \
	grep \
	${_rcs} \
	sort \
	${_texinfo}

.if ${MK_CXX} != "no"
_gperf=		gperf
.endif

.if ${MK_CVS} != "no"
_cvs=		cvs
.endif

.if ${MK_FDT}	!= "no"
_dtc=		dtc
.endif

.if ${MK_INFO} != "no"
_texinfo=	texinfo
.endif

.if ${MK_RCS} != "no"
_rcs=		rcs
.endif

.if ${MK_BINUTILS} != "no"
_binutils=	binutils
.endif
.if ${MK_GCC} != "no"
_cc=		cc
.endif
.if ${MK_GDB} != "no"
_gdb=		gdb
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
