# $FreeBSD: release/10.0.0/tools/tools/ath/athspectral/Makefile 244953 2013-01-02 04:02:27Z adrian $

PROG=	athspectral

.include <../Makefile.inc>

SRCS=	athspectral.c
SRCS+=	opt_ah.h
CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

.include <bsd.prog.mk>
