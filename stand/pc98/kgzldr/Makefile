# $FreeBSD: stable/11/stand/pc98/kgzldr/Makefile 344421 2019-02-21 06:02:51Z kevans $

.include <bsd.init.mk>

PROG=	kgzldr.o
STRIP=
BINMODE=${LIBMODE}
BINDIR=	${LIBDIR}

SRCS=	start.s boot.c subr_inflate.c lib.c crt.s sio.s
CFLAGS=	-Os
CFLAGS+=-DKZIP
NO_SHARED=
LDFLAGS+=-Wl,-r
.PATH:	${SYSDIR}/kern
.PATH:	${BOOTSRC}/i386/kgzldr

BOOT_COMCONSOLE_PORT?=	0x238
AFLAGS+=--defsym SIO_PRT=${BOOT_COMCONSOLE_PORT}

.include <bsd.prog.mk>
