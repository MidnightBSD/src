# $FreeBSD: release/7.0.0/tools/tools/sysdoc/Makefile 142871 2005-03-01 05:48:37Z trhodes $

MAINTAINER=	trhodes@FreeBSD.org

sysctl.5:
	sh ${.CURDIR}/sysdoc.sh -k /boot/kernel

MAN=	sysctl.5

CLEANFILES=	tunables.TODO markup.file sysctl.5 _names

.include <bsd.prog.mk>
