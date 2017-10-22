# $FreeBSD: stable/9/tools/tools/sysdoc/Makefile 242112 2012-10-26 00:44:32Z eadler $

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
