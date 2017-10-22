# $FreeBSD: release/10.0.0/sys/modules/aesni/Makefile 255452 2013-09-10 16:50:13Z obrien $

.PATH: ${.CURDIR}/../../crypto/aesni

KMOD=	aesni
SRCS=	aesni.c
SRCS+=	aeskeys_${MACHINE_CPUARCH}.S
SRCS+=	device_if.h bus_if.h opt_bus.h cryptodev_if.h

OBJS+=	aesni_wrap.o

# Remove -nostdinc so we can get the intrinsics.
aesni_wrap.o: aesni_wrap.c
	${CC} -c ${CFLAGS:C/^-O2$/-O3/:N-nostdinc} ${WERROR} ${PROF} \
	     -mmmx -msse -maes ${.IMPSRC}
	${CTFCONVERT_CMD}

.include <bsd.kmod.mk>

