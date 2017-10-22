# $FreeBSD: stable/9/sys/boot/efi/Makefile.inc 180012 2008-06-25 21:33:28Z ru $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib

.include "../Makefile.inc"
