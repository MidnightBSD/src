# $FreeBSD: release/10.0.0/tools/tools/ath/athradar/Makefile 224266 2011-07-22 09:34:31Z adrian $

PROG=	athradar

.include <../Makefile.inc>

SRCS=	athradar.c
SRCS+=	opt_ah.h
CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

.include <bsd.prog.mk>
