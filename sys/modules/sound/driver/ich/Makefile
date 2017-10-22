# $FreeBSD: stable/9/sys/modules/sound/driver/ich/Makefile 246069 2013-01-29 13:49:08Z pfg $

.PATH:		${.CURDIR}/../../../../dev/sound/pci
KMOD		= snd_ich
SRCS		= device_if.h bus_if.h pci_if.h
SRCS		+= ich.c

.include <bsd.kmod.mk>
