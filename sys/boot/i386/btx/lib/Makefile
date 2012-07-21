# $MidnightBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.2 2011/10/16 21:11:08 laffer1 Exp $
# $FreeBSD: src/sys/boot/i386/btx/lib/Makefile,v 1.13.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	crt0.o
INTERNALPROG=
NO_MAN=
SRCS=	btxcsu.S btxsys.s btxv86.s
CFLAGS+=-I${.CURDIR}/../../common
LDFLAGS=-Wl,-r

.include <bsd.prog.mk>
