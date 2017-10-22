# $FreeBSD: release/10.0.0/tools/tools/ath/athprom/Makefile 220534 2011-04-11 06:53:45Z adrian $

PROG=	athprom

.include <../Makefile.inc>

TEMPLATEDIR=	/usr/local/libdata/athprom
TEXTMODE?=	444

SRCS=	athprom.c
SRCS+=	opt_ah.h
CLEANFILES+=	opt_ah.h

opt_ah.h:
	echo "#define AH_DEBUG 1" > opt_ah.h
	echo "#define AH_DEBUG_COUNTRY 1" >> opt_ah.h
	echo "#define AH_SUPPORT_AR5416 1" >> opt_ah.h

CFLAGS+=-DDIR_TEMPLATE='"${TEMPLATEDIR}"'

beforeinstall:
	mkdir -p ${DESTDIR}${TEMPLATEDIR}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${TEXTMODE} \
	    ${.CURDIR}/eeprom-* ${DESTDIR}${TEMPLATEDIR}/

.include <bsd.prog.mk>
