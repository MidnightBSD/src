# $FreeBSD: stable/9/sys/modules/hpt27xx/Makefile 245938 2013-01-26 05:20:09Z delphij $
HPT27XX=	${.CURDIR}/../../dev/hpt27xx
.PATH:	${HPT27XX}

KMOD=	hpt27xx
SRCS=	bus_if.h device_if.h pci_if.h
SRCS+=	opt_cam.h opt_scsi.h
SRCS+=	os_bsd.h os_bsd.c osm_bsd.c hpt27xx_config.c
OBJS+=	hpt27xx_lib.o

hpt27xx_lib.o:
	uudecode -p < ${HPT27XX}/$(MACHINE_ARCH)-elf.hpt27xx_lib.o.uu > ${.TARGET}

.include <bsd.kmod.mk>
