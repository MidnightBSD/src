# $FreeBSD: release/10.0.0/tools/tools/ath/athrd/Makefile 187322 2009-01-15 23:38:21Z sam $

.PATH:	${.CURDIR}/../../../../sys/dev/ath/ath_hal

PROG=	athrd

SRCS=	athrd.c ah_regdomain.c opt_ah.h

CLEANFILES+=	opt_ah.h

CFLAGS+= -fno-inline

.include <../Makefile.inc>

MAN=	athrd.1

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h

.include <bsd.prog.mk>
