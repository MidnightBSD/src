# $FreeBSD: stable/11/sys/modules/firewire/sbp_targ/Makefile 319182 2017-05-30 04:11:12Z ngie $

# Makefile for the SBP-II Target mode

.PATH: ${SRCTOP}/sys/dev/firewire

KMOD	= sbp_targ
SRCS	= bus_if.h device_if.h \
	  opt_cam.h opt_scsi.h \
	  sbp_targ.c sbp.h \
	  firewire.h firewirereg.h \
	  iec13213.h

.include <bsd.kmod.mk>
