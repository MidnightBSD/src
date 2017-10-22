# $FreeBSD: stable/9/sys/boot/pc98/btx/lib/Makefile 237763 2012-06-29 10:12:18Z avg $

PROG=	crt0.o
INTERNALPROG=
NO_MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../../i386/common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
