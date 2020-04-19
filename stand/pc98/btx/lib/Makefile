# $FreeBSD: stable/11/stand/pc98/btx/lib/Makefile 330311 2018-03-03 06:37:53Z kevans $

.include <bsd.init.mk>

PROG=	crt0.o
INTERNALPROG=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${BOOTSRC}/i386/common
LDFLAGS+=-Wl,-r

.include <bsd.prog.mk>
