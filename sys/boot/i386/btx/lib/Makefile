# $FreeBSD: release/10.0.0/sys/boot/i386/btx/lib/Makefile 235154 2012-05-09 08:04:29Z avg $

PROG=	crt0.o
INTERNALPROG=
NO_MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
