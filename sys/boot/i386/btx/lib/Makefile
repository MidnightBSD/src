# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/i386/btx/lib/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	crt0.o
INTERNALPROG=
MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
