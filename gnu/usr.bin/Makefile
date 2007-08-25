# $MidnightBSD: src/gnu/usr.bin/Makefile,v 1.5 2007/07/22 09:41:35 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/Makefile,v 1.84 2005/03/21 05:30:58 kientzle Exp $

SUBDIR= ${_binutils} \
	${_cc} \
	${_cvs} \
	dialog \
	diff \
	diff3 \
	${_gdb} \
	${_gperf} \
	grep \
	${_groff} \
	man \
	patch \
	rcs \
	sort \
	texinfo
 
.if ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "powerpc"
NO_GDB=	# not yet
.endif

.if !defined(NO_CXX)
.if ${MACHINE_ARCH} != "powerpc"
_gperf=		gperf
.endif
_groff=		groff
.endif

.if !defined(NO_CVS)
_cvs=		cvs
.endif

.if !defined(NO_TOOLCHAIN)
_binutils=	binutils
_cc=		cc
.if !defined(NO_GDB)
_gdb=		gdb
.endif
.endif

.include <bsd.subdir.mk>
