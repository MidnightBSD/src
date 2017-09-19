# $MidnightBSD$
# $FreeBSD: release/9.2.0/sys/modules/nvme/Makefile 253631 2013-07-24 22:48:29Z jimharris $

.PATH: ${.CURDIR}/../../dev/nvme

KMOD = nvme

SRCS =	nvme.c			\
	nvme_ctrlr.c		\
	nvme_ctrlr_cmd.c	\
	nvme_ns.c		\
	nvme_ns_cmd.c		\
	nvme_qpair.c		\
	nvme_sysctl.c		\
	nvme_test.c		\
	nvme_util.c		\
				\
	bus_if.h		\
	device_if.h		\
	pci_if.h

.include <bsd.kmod.mk>
