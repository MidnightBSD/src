# $FreeBSD: stable/9/tools/tools/ath/ath_prom_read/Makefile 217739 2011-01-22 23:44:56Z adrian $

PROG=	ath_prom_read

.include <bsd.prog.mk>

.include <../Makefile.inc>

CLEANFILES+= opt_ah.h ah_osdep.h

CFLAGS+=-DATH_SUPPORT_ANI
CFLAGS+=-DATH_SUPPORT_TDMA

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${SRCDIR}/sys/net80211

opt_ah.h:
	touch opt_ah.h
ah_osdep.h:
	echo 'typedef void *HAL_SOFTC;' >ah_osdep.h
	echo 'typedef int HAL_BUS_TAG;' >>ah_osdep.h
	echo 'typedef void *HAL_BUS_HANDLE;' >>ah_osdep.h
