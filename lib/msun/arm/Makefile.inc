# $MidnightBSD$
# $FreeBSD: stable/10/lib/msun/arm/Makefile.inc 266314 2014-05-17 14:22:37Z ian $

LDBL_PREC = 53
SYM_MAPS += ${.CURDIR}/arm/Symbol.map

.if ${TARGET_ARCH} == "armv6"
ARCH_SRCS = fenv-softfp.c fenv-vfp.c
.endif

CFLAGS.fenv-vfp.c=	-mfloat-abi=softfp
CFLAGS+=		${CFLAGS.${.IMPSRC:T}}

