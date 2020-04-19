# $FreeBSD: stable/11/stand/efi/loader/arch/amd64/Makefile.inc 329140 2018-02-11 20:58:00Z kevans $

SRCS+=	amd64_tramp.S \
	start.S \
	elf64_freebsd.c \
	trap.c \
	exc.S

.PATH:	${BOOTSRC}/i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c \
	spinconsole.c

CFLAGS+=	-fPIC -DTERM_EMU
LDFLAGS+=	-Wl,-znocombreloc
