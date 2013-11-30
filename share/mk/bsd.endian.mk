# $MidnightBSD$
# $FreeBSD: src/share/mk/bsd.endian.mk,v 1.4 2006/11/05 15:33:26 cognet Exp $

.if ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "ia64" || \
    (${MACHINE_ARCH} == "arm" && !defined(TARGET_BIG_ENDIAN))
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "arm"
TARGET_ENDIANNESS= 4321
.endif
