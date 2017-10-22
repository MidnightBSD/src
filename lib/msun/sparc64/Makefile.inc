# $FreeBSD: release/10.0.0/lib/msun/sparc64/Makefile.inc 157196 2006-03-27 23:59:45Z deischen $

ARCH_SRCS= e_sqrt.S e_sqrtf.S
LDBL_PREC= 113
SYM_MAPS+= ${.CURDIR}/sparc64/Symbol.map
