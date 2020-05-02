# $FreeBSD: stable/11/lib/msun/arm/Makefile.inc 300119 2016-05-18 06:01:18Z imp $

LDBL_PREC = 53
SYM_MAPS += ${.CURDIR}/arm/Symbol.map

.if ${MACHINE_ARCH:Marmv6*} && defined(CPUTYPE) && ${CPUTYPE:M*soft*} != ""
ARCH_SRCS = fenv-softfp.c fenv-vfp.c
.endif

CFLAGS.fenv-vfp.c=	-mfpu=vfp -mfloat-abi=softfp
