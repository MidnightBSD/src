# $FreeBSD: release/10.0.0/sys/boot/pc98/btx/lib/Makefile 235264 2012-05-11 09:46:17Z avg $

PROG=	crt0.o
INTERNALPROG=
NO_MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../../i386/common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
