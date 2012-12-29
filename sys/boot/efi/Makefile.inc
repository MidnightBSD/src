# $MidnightBSD: src/sys/boot/efi/Makefile.inc,v 1.2 2008/10/01 16:41:13 laffer1 Exp $

BINDIR?=	/boot

# Options used when building app-specific efi components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
