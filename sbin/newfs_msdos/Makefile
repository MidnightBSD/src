# $FreeBSD: release/10.0.0/sbin/newfs_msdos/Makefile 211725 2010-08-23 22:24:11Z imp $

PROG=	newfs_msdos
MAN=	newfs_msdos.8

# XXX - this is verboten
.if ${MACHINE_CPUARCH} == "arm"
WARNS?= 3
.endif

.include <bsd.prog.mk>
