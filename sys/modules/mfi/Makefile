# $FreeBSD: stable/11/sys/modules/mfi/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/mfi

SUBDIR= mfip

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+= mfi_linux
.endif

KMOD=	mfi
SRCS=	mfi.c mfi_pci.c mfi_disk.c mfi_debug.c mfi_syspd.c mfi_tbolt.c
SRCS+=	opt_mfi.h opt_cam.h opt_compat.h
SRCS+=	device_if.h bus_if.h pci_if.h

#CFLAGS += -DMFI_DEBUG

.include <bsd.kmod.mk>
