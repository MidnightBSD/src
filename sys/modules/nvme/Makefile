# $FreeBSD: stable/11/sys/modules/nvme/Makefile 329095 2018-02-10 00:39:34Z mav $

.PATH: ${SRCTOP}/sys/dev/nvme

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
	opt_cam.h		\
	opt_nvme.h		\
	pci_if.h

.include <bsd.kmod.mk>
