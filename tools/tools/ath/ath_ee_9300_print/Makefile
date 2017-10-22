# $FreeBSD: release/10.0.0/tools/tools/ath/ath_ee_9300_print/Makefile 251738 2013-06-14 08:12:40Z adrian $

.PATH:	${.CURDIR}/../../../../sys/dev/ath/ath_hal

PROG=	ath_ee_9300_print
NOMAN=	yes
NO_MAN=	yes
SRCS=	main.c
SRCS+=	opt_ah.h
CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

.include <../Makefile.inc>

.include <bsd.prog.mk>
