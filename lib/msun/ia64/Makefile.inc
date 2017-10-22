# $FreeBSD: release/10.0.0/lib/msun/ia64/Makefile.inc 157196 2006-03-27 23:59:45Z deischen $

ARCH_SRCS = s_fma.S s_fmaf.S s_fmal.S
LDBL_PREC = 64
SYM_MAPS += ${.CURDIR}/ia64/Symbol.map
