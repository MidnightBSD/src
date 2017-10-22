# $FreeBSD: release/7.0.0/sys/boot/ia64/Makefile.inc 125729 2004-02-12 08:10:34Z ru $

BINDIR?=	/boot

# Options used when building standalone components
CFLAGS+=	-ffreestanding -fshort-wchar -Wformat
LDFLAGS+=	-nostdlib
