# $FreeBSD: release/10.0.0/sys/boot/i386/libfirewire/Makefile 211677 2010-08-23 01:42:09Z imp $

LIB=		firewire
INTERNALLIB=

.PATH:		${.CURDIR}/../../../dev/dcons ${.CURDIR}/../../../dev/firewire
SRCS+=		firewire.c fwohci.c dconsole.c
SRCS+=		dcons.c fwcrom.c

CFLAGS+=	-D_BOOT

CFLAGS+=	-I${.CURDIR}/../../common -I${.CURDIR}/../../.. -I.
CFLAGS+=	-I${.CURDIR}/../../../../lib/libstand
CFLAGS+=	-I${.CURDIR}/../btx/lib
CFLAGS+=	-I${.CURDIR}/../libi386

CFLAGS+=	-Wformat -Wall

.if ${MACHINE_CPUARCH} == "amd64"
CLEANFILES+=    machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.lib.mk>

.if ${MACHINE_CPUARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif

