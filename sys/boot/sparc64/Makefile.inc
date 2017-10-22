# $FreeBSD: release/7.0.0/sys/boot/sparc64/Makefile.inc 125622 2004-02-09 14:17:02Z ru $

BINDIR?=	/boot
CFLAGS+=	-ffreestanding
LDFLAGS+=	-nostdlib
