# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/i386/kgzldr/Makefile 276486 2014-12-31 23:25:37Z ngie $

PROG=	kgzldr.o
STRIP=
BINMODE=${LIBMODE}
BINDIR=	${LIBDIR}
MAN=

SRCS=	start.s boot.c inflate.c lib.c crt.s sio.s
CFLAGS=	-Os
CFLAGS+=-DKZIP
NO_SHARED=
LDFLAGS=-Wl,-r
.PATH:	${.CURDIR}/../../../kern

BOOT_COMCONSOLE_PORT?=	0x3f8
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

.include <bsd.prog.mk>
