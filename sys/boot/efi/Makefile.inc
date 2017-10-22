# $FreeBSD: release/7.0.0/sys/boot/efi/Makefile.inc 125729 2004-02-12 08:10:34Z ru $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
