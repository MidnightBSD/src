# $FreeBSD: release/10.0.0/tools/tools/ath/athsurvey/Makefile 234751 2012-04-28 08:17:19Z adrian $

PROG=	athsurvey

.include <../Makefile.inc>

SRCS=	athsurvey.c
SRCS+=	opt_ah.h
CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

.include <bsd.prog.mk>
