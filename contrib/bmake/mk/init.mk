# $Id: init.mk,v 1.9 2013/07/18 05:46:24 sjg Exp $
#
#	@(#) Copyright (c) 2002, Simon J. Gerraty
#
#	This file is provided in the hope that it will
#	be of use.  There is absolutely NO WARRANTY.
#	Permission to copy, redistribute or otherwise
#	use this file is hereby granted provided that 
#	the above copyright notice and this notice are
#	left intact. 
#      
#	Please send copies of changes and bug-fixes to:
#	sjg@crufty.net
#

.if !target(__${.PARSEFILE}__)
__${.PARSEFILE}__:

.if ${MAKE_VERSION:U0} > 20100408
_this_mk_dir := ${.PARSEDIR:tA}
.else
_this_mk_dir := ${.PARSEDIR}
.endif

.-include <local.init.mk>
.-include "${.CURDIR:H}/Makefile.inc"
.include <own.mk>

.MAIN:		all

.if !empty(WARNINGS_SET) || !empty(WARNINGS_SET_${MACHINE_ARCH})
.include <warnings.mk>
.endif

COPTS += ${COPTS.${.IMPSRC:T}}
CPPFLAGS += ${CPPFLAGS.${.IMPSRC:T}}
CPUFLAGS += ${CPUFLAGS.${.IMPSRC:T}}

CC_PG?= -pg
CXX_PG?= ${CC_PG}
CC_PIC?= -DPIC
CXX_PIC?= ${CC_PIC}
PROFFLAGS?= -DGPROF -DPROF

.if ${.MAKE.LEVEL:U1} == 0 && ${BUILD_AT_LEVEL0:Uyes:tl} == "no"
# this tells lib.mk and prog.mk to not actually build anything
_SKIP_BUILD = not building at level 0
.endif

.endif
