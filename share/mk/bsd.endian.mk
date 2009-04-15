# $MidnightBSD: src/share/mk/bsd.endian.mk,v 1.4 2008/04/28 05:53:25 laffer1 Exp $
# $FreeBSD: src/share/mk/bsd.endian.mk,v 1.4 2006/11/05 15:33:26 cognet Exp $

.if ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "i386"
TARGET_ENDIANNESS= 1234
.elif ${MACHINE_ARCH} == "sparc64"
TARGET_ENDIANNESS= 4321
.endif
