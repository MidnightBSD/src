# $FreeBSD: stable/11/lib/csu/Makefile 283125 2015-05-19 21:00:53Z emaste $

.if exists(${.CURDIR}/${MACHINE_ARCH})
SUBDIR+= ${MACHINE_ARCH}
.else
SUBDIR+= ${MACHINE_CPUARCH}
.endif

.include <bsd.subdir.mk>
