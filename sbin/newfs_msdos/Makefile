# $FreeBSD: stable/11/sbin/newfs_msdos/Makefile 298107 2016-04-16 07:45:30Z gjb $

PACKAGE=runtime
PROG=	newfs_msdos
MAN=	newfs_msdos.8
SRCS=	newfs_msdos.c mkfs_msdos.c

# XXX - this is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif

.include <bsd.prog.mk>
