# $FreeBSD: release/10.0.0/sys/boot/ofw/libofw/Makefile 235364 2012-05-12 20:27:33Z avg $

LIB=		ofw
INTERNALLIB=

SRCS=	devicename.c elf_freebsd.c ofw_console.c ofw_copy.c ofw_disk.c \
	ofw_memory.c ofw_module.c ofw_net.c ofw_reboot.c \
	ofw_time.c openfirm.c
.PATH:	${.CURDIR}/../../zfs
SRCS+=  devicename_stubs.c

CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand/

# Pick up the bootstrap header for some interface items
CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.

CFLAGS+=	-ffreestanding
.if ${MACHINE_CPUARCH} == "powerpc"
CFLAGS+=	-msoft-float
SRCS+=		ppc64_elf_freebsd.c
.endif

.ifdef(BOOT_DISK_DEBUG)
# Make the disk code more talkative
CFLAGS+= -DDISK_DEBUG
.endif

machine:
	ln -sf ${.CURDIR}/../../../${MACHINE_CPUARCH}/include machine

CLEANFILES+=	machine

.include <bsd.lib.mk>

beforedepend ${OBJS}: machine
