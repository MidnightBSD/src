# $MidnightBSD$
# $FreeBSD: stable/10/gnu/usr.bin/Makefile 322094 2017-08-05 12:33:00Z marius $

.include <bsd.own.mk>

SUBDIR= ${_binutils} \
	${_cc} \
	${_dialog} \
	diff \
	${_dtc} \
	${_gdb} \
	${_gperf} \
	grep \
	${_groff} \
	${_rcs} \
	${_tests} \
	${_tests} \
	${_texinfo}

SUBDIR_DEPEND_gdb=	${_binutils}

.if ${MK_CXX} != "no"
_gperf=		gperf
.endif

.if ${MK_GROFF} != "no"
_groff=		groff
.endif

.if ${MK_DIALOG} != "no"
_dialog=	dialog
.endif

.if ${MK_GPL_DTC} != "no"
_dtc=		dtc
.endif

.if ${MK_INFO} != "no"
_texinfo=	texinfo
.endif

.if ${MK_RCS} != "no"
_rcs=		rcs
.endif

.if ${MK_TESTS} != "no"
_tests=		tests
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
