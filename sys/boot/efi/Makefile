# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/efi/Makefile 295538 2016-02-11 17:56:09Z smh $

.include <bsd.own.mk>

# In-tree GCC does not support __attribute__((ms_abi)).
.if ${COMPILER_TYPE} != "gcc"

.if ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	libefi loader boot1
.endif

.endif # ${COMPILER_TYPE} != "gcc"

.if ${MACHINE_CPUARCH} == "ia64"
SUBDIR+=	libefi
.endif

.include <bsd.subdir.mk>

