# $FreeBSD: release/10.0.0/tools/tools/sysdoc/Makefile 241828 2012-10-22 02:12:11Z eadler $

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
