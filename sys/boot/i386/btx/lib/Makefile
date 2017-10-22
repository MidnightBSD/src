# $FreeBSD: stable/9/sys/boot/i386/btx/lib/Makefile 237756 2012-06-29 10:01:56Z avg $

PROG=	crt0.o
INTERNALPROG=
NO_MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
