# $MidnightBSD$

# $FreeBSD: stable/10/sys/modules/aic7xxx/Makefile 270284 2014-08-21 17:18:21Z ian $

SUBDIR= ahc ahd

firmware: aicasm/aicasm ahdfirmware ahcfirmware

aicasm/aicasm:
	( cd ${.CURDIR}/aicasm ; ${MAKE} aicasm )

ahdfirmware:
	( cd ${.CURDIR}/ahd ; ${MAKE} ahdfirmware )

ahcfirmware:
	( cd ${.CURDIR}/ahc ; ${MAKE} ahcfirmware )

cleanfirmware:
	( cd .${CURDIR}/aicasm ; ${MAKE} clean )
	( cd .${CURDIR}/ahc ; ${MAKE} cleanfirmware )
	( cd .${CURDIR}/ahd ; ${MAKE} cleanfirmware )

.include <bsd.subdir.mk>
