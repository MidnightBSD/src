# $FreeBSD: stable/11/stand/efi/loader/arch/i386/Makefile.inc 329140 2018-02-11 20:58:00Z kevans $

SRCS+=	start.S \
	efimd.c \
	elf32_freebsd.c \
	exec.c

.PATH:	${BOOTSRC}/i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c \
	spinconsole.c

CFLAGS+=	-fPIC -DTERM_EMU
LDFLAGS+=	-Wl,-znocombreloc
