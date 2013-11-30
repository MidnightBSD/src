# $FreeBSD: src/share/mk/bsd.endian.mk,v 1.2 2005/02/25 00:24:03 cognet Exp $
# $MidnightBSD: src/share/mk/bsd.endian.mk,v 1.2 2006/05/22 06:03:21 laffer1 Exp $

.if ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64" || \
    (${MACHINE_ARCH} == "arm" && !defined(ARM_BIG_ENDIAN))
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "arm"
TARGET_ENDIANNESS= 4321
.endif
