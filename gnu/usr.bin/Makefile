# $FreeBSD: stable/11/gnu/usr.bin/Makefile 332485 2018-04-13 19:15:31Z ian $

.include <src.opts.mk>

.if ${MK_CXX} != "no"
SUBDIR.${MK_GCC}+=	gperf
SUBDIR.${MK_GROFF}+=	groff
.endif

SUBDIR.${MK_BINUTILS}+=	binutils
SUBDIR.${MK_DIALOG}+=	dialog

.if ${MK_BINUTILS} != "no"
SUBDIR.${MK_GDB}+=	gdb
SUBDIR_DEPEND_gdb=	binutils
.endif

SUBDIR.${MK_GCC}+=	cc
SUBDIR.${MK_GNU_DIFF}+=	diff
SUBDIR.${MK_GNU_GREP}+=	grep
SUBDIR.${MK_GPL_DTC}+=	dtc
SUBDIR.${MK_RCS}+=	rcs
SUBDIR.${MK_TESTS}+=	tests

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
