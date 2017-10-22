# $FreeBSD: release/10.0.0/tools/tools/ath/athregs/Makefile 220534 2011-04-11 06:53:45Z adrian $

PROG=	athregs

.PATH.c: ${.CURDIR}/../common

SRCS=	dumpregs.c
SRCS+=	dumpregs_5210.c
SRCS+=	dumpregs_5211.c
SRCS+=	dumpregs_5212.c
SRCS+=	dumpregs_5416.c
SRCS+=	opt_ah.h

CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

.include <../Makefile.inc>

.include <bsd.prog.mk>
