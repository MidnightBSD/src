# $FreeBSD: release/10.0.0/sys/modules/nve/Makefile 228861 2011-12-24 13:36:28Z dim $

.PATH: ${.CURDIR}/../../dev/nve

KMOD=	if_nve
SRCS=	if_nve.c if_nvereg.h miidevs.h \
	device_if.h bus_if.h pci_if.h miibus_if.h \
	os+%DIKED-nve.h
OBJS+=	nvenetlib.o
WERROR=

CLEANFILES+=	nvenetlib.o os+%DIKED-nve.h
nvenetlib.o: ${.CURDIR}/../../contrib/dev/nve/${MACHINE}/${.TARGET}.bz2.uu
	uudecode < ${.CURDIR}/../../contrib/dev/nve/${MACHINE}/${.TARGET}.bz2.uu
	bzip2 -df ${.TARGET}.bz2

os+%DIKED-nve.h: ${.CURDIR}/../../contrib/dev/nve/os.h
	sed -e 's/^.*#include.*phy\.h.*$$//' ${.OODATE} > ${.TARGET}

.include <bsd.kmod.mk>
