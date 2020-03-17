
# $FreeBSD: stable/11/sys/modules/aic7xxx/Makefile 260401 2014-01-07 19:33:17Z scottl $

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
