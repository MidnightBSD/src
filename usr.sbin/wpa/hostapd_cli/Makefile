# $FreeBSD: stable/9/usr.sbin/wpa/hostapd_cli/Makefile 189270 2009-03-02 04:11:34Z sam $

.include "${.CURDIR}/../Makefile.inc"

.PATH.c:${HOSTAPD_DISTDIR}

PROG=	hostapd_cli
SRCS=	hostapd_cli.c wpa_ctrl.c os_unix.c

CFLAGS+= -DCONFIG_CTRL_IFACE
CFLAGS+= -DCONFIG_CTRL_IFACE_UNIX

MAN=	hostapd_cli.8

.include <bsd.prog.mk>
