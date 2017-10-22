# $FreeBSD: release/7.0.0/sys/boot/i386/libfirewire/Makefile 170101 2007-05-29 14:35:57Z simokawa $

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

.if ${MACHINE_ARCH} == "amd64"
CLEANFILES+=    machine
machine:
	ln -sf ${.CURDIR}/../../../i386/include machine
.endif

.include <bsd.lib.mk>

.if ${MACHINE_ARCH} == "amd64"
beforedepend ${OBJS}: machine
.endif

