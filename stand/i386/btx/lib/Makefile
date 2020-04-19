# $FreeBSD: stable/11/stand/i386/btx/lib/Makefile 329183 2018-02-12 20:51:28Z kevans $

.include <bsd.init.mk>

PROG=	crt0.o
INTERNALPROG=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${BOOTSRC}/i386/common
LDFLAGS+=-Wl,-r

.include <bsd.prog.mk>
