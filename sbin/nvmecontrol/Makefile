# $FreeBSD: release/10.0.0/sbin/nvmecontrol/Makefile 253476 2013-07-19 21:40:57Z jimharris $

PROG=	nvmecontrol
SRCS=	nvmecontrol.c devlist.c firmware.c identify.c logpage.c	\
	perftest.c reset.c nvme_util.c
MAN=	nvmecontrol.8

.PATH:	${.CURDIR}/../../sys/dev/nvme

.include <bsd.prog.mk>
