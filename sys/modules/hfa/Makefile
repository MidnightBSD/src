# $FreeBSD: release/7.0.0/sys/modules/hfa/Makefile 94898 2002-04-17 00:26:09Z mdodd $

SUBDIR=	hfa pci

.ifdef notyet
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=eisa
.endif

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR+=sbus
.endif
.endif

.include <bsd.subdir.mk>
