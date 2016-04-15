# $MidnightBSD$
# $FreeBSD: stable/10/share/mk/bsd.progs.mk 291791 2015-12-04 18:09:51Z bdrewery $
# $Id: progs.mk,v 1.11 2012/11/06 17:18:54 sjg Exp $
#
#	@(#) Copyright (c) 2006, Simon J. Gerraty
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

.MAIN: all

.if defined(PROGS) || defined(PROGS_CXX)
# we really only use PROGS below...
PROGS += ${PROGS_CXX}

# In meta mode, we can capture dependenices for _one_ of the progs.
# if makefile doesn't nominate one, we use the first.
.if defined(.PARSEDIR)
.ifndef UPDATE_DEPENDFILE_PROG
UPDATE_DEPENDFILE_PROG = ${PROGS:[1]}
.export UPDATE_DEPENDFILE_PROG
.endif
.else
UPDATE_DEPENDFILE_PROG?= no
.endif

.ifndef PROG
# They may have asked us to build just one
.for t in ${PROGS}
.if make($t)
.if ${PROGS_CXX:U:M${t}}
PROG_CXX ?= $t
.endif
PROG ?= $t
.endif
.endfor
.endif

.if defined(PROG)
# just one of many
PROG_OVERRIDE_VARS +=	BINDIR BINGRP BINOWN BINMODE DPSRCS MAN NO_WERROR \
			PROGNAME SRCS WARNS
PROG_VARS +=	CFLAGS CPPFLAGS CXXFLAGS DPADD DPLIBS LDADD LINKS \
		LDFLAGS MLINKS ${PROG_OVERRIDE_VARS}
.for v in ${PROG_VARS:O:u}
.if empty(${PROG_OVERRIDE_VARS:M$v})
.if defined(${v}.${PROG})
$v += ${${v}.${PROG}}
.elif defined(${v}_${PROG})
$v += ${${v}_${PROG}}
.endif
.else
$v ?=
.endif
.endfor

# for meta mode, there can be only one!
.if ${PROG} == ${UPDATE_DEPENDFILE_PROG}
UPDATE_DEPENDFILE ?= yes
.endif
UPDATE_DEPENDFILE ?= NO

# ensure that we don't clobber each other's dependencies
DEPENDFILE?= .depend.${PROG}
# prog.mk will do the rest
.else # !defined(PROG)
all: ${PROGS}

# We cannot capture dependencies for meta mode here
UPDATE_DEPENDFILE = NO
# nor can we safely run in parallel.
.NOTPARALLEL:
.endif
.endif	# PROGS || PROGS_CXX

# These are handled by the main make process.
.ifdef _RECURSING_PROGS
_PROGS_GLOBAL_VARS= CLEANFILES CLEANDIRS FILESGROUPS SCRIPTS
.for v in ${_PROGS_GLOBAL_VARS}
$v =
.endfor
.endif

# handle being called [bsd.]progs.mk
.include <bsd.prog.mk>

.if !empty(PROGS) && !defined(_RECURSING_PROGS) && !defined(PROG)
# tell progs.mk we might want to install things
PROGS_TARGETS+= checkdpadd clean cleandepend cleandir depend install

# Find common sources among the PROGS and depend on them before building
# anything.  This allows parallelization without them each fighting over
# the same objects.
_PROGS_COMMON_SRCS=
_PROGS_ALL_SRCS=
.for p in ${PROGS}
.for s in ${SRCS.${p}}
.if ${_PROGS_ALL_SRCS:M${s}} && !${_PROGS_COMMON_SRCS:M${s}}
_PROGS_COMMON_SRCS+=	${s}
.else
_PROGS_ALL_SRCS+=	${s}
.endif
.endfor
.endfor
.if !empty(_PROGS_COMMON_SRCS)
_PROGS_COMMON_OBJS=	${_PROGS_COMMON_SRCS:M*.[dhly]}
.if !empty(_PROGS_COMMON_SRCS:N*.[dhly])
_PROGS_COMMON_OBJS+=	${_PROGS_COMMON_SRCS:N*.[dhly]:R:S/$/.o/g}
.endif
${PROGS}: ${_PROGS_COMMON_OBJS}
.endif

.for p in ${PROGS}
.if defined(PROGS_CXX) && !empty(PROGS_CXX:M$p)
# bsd.prog.mk may need to know this
x.$p= PROG_CXX=$p
.endif

# Main PROG target
$p ${p}_p: .PHONY .MAKE
	(cd ${.CURDIR} && \
	    NO_SUBDIR=1 ${MAKE} -f ${MAKEFILE} _RECURSING_PROGS= \
	    PROG=$p \
	    DEPENDFILE=.depend.$p .MAKE.DEPENDFILE=.depend.$p \
	    ${x.$p})

# Pseudo targets for PROG, such as 'install'.
.for t in ${PROGS_TARGETS:O:u}
$p.$t: .PHONY .MAKE
	(cd ${.CURDIR} && \
	    NO_SUBDIR=1 ${MAKE} -f ${MAKEFILE} _RECURSING_PROGS= \
	    PROG=$p \
	    DEPENDFILE=.depend.$p .MAKE.DEPENDFILE=.depend.$p \
	    ${x.$p} ${@:E})
.endfor
.endfor

# Depend main pseudo targets on all PROG.pseudo targets too.
.for t in ${PROGS_TARGETS:O:u}
$t: ${PROGS:%=%.$t}
.endfor
.endif	# !empty(PROGS) && !defined(_RECURSING_PROGS) && !defined(PROG)
