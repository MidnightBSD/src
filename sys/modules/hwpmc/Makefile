.PATH: ${SRCTOP}/sys/dev/hwpmc

KMOD=	hwpmc

SRCS=	hwpmc_mod.c hwpmc_logging.c hwpmc_soft.c vnode_if.h

.if ${MACHINE_CPUARCH} == "aarch64"
SRCS+=  hwpmc_arm64.c hwpmc_arm64_md.c
.endif

.if ${MACHINE_CPUARCH} == "amd64"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c hwpmc_tsc.c
SRCS+=	hwpmc_x86.c hwpmc_uncore.c
SRCS+=	device_if.h bus_if.h
.endif

.if ${MACHINE_CPUARCH} == "arm"
SRCS+=	hwpmc_arm.c
.endif

.if ${MACHINE_ARCH} == "armv7"
SRCS+= hwpmc_armv7.c
.endif

.if ${MACHINE_CPUARCH} == "i386"
SRCS+=	hwpmc_amd.c hwpmc_core.c hwpmc_intel.c
SRCS+=	hwpmc_tsc.c hwpmc_x86.c hwpmc_uncore.c
SRCS+=	device_if.h bus_if.h
.endif

.include <bsd.kmod.mk>
