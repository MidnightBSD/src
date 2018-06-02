# $MidnightBSD$
# $FreeBSD: stable/10/sys/boot/efi/loader/arch/amd64/Makefile.inc 293297 2016-01-07 02:22:45Z emaste $

SRCS+=	amd64_tramp.S \
	start.S \
	framebuffer.c \
	elf64_freebsd.c \
	reloc.c

.PATH:	${.CURDIR}/../../i386/libi386
SRCS+=	nullconsole.c \
	comconsole.c
