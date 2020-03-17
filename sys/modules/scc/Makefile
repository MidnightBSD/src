# $FreeBSD: stable/11/sys/modules/scc/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/dev/scc

.if ${MACHINE} == "sparc64"
scc_bfe= scc_bfe_ebus.c scc_bfe_sbus.c
.endif
.if ${MACHINE_CPUARCH} == "powerpc"
scc_bfe= scc_bfe_macio.c scc_bfe_quicc.c scc_dev_quicc.c
.endif

KMOD=	scc
SRCS=	${scc_bfe} scc_core.c scc_if.c scc_if.h \
	scc_dev_sab82532.c scc_dev_z8530.c
SRCS+=	bus_if.h device_if.h ofw_bus_if.h serdev_if.c serdev_if.h

.include <bsd.kmod.mk>
