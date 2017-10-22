# $FreeBSD: release/10.0.0/sys/boot/ia64/Makefile.inc 180012 2008-06-25 21:33:28Z ru $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
