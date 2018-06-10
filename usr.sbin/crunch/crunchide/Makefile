# $MidnightBSD$
# $FreeBSD: stable/10/usr.sbin/crunch/crunchide/Makefile 292423 2015-12-18 03:06:39Z emaste $

PROG=   crunchide
SRCS=	crunchide.c exec_elf32.c exec_elf64.c

CFLAGS+=-DNLIST_ELF32 -DNLIST_ELF64
exec_elf64.o: exec_elf32.c

.include <bsd.prog.mk>
