# $FreeBSD: release/10.0.0/lib/csu/Makefile 250863 2013-05-21 17:47:53Z marcel $

ARCH= ${MACHINE_ARCH:S/i386/i386-elf/}
.if exists(${.CURDIR}/${ARCH})
SUBDIR+= ${ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

.include <bsd.subdir.mk>
