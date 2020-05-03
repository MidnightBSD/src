# $FreeBSD: stable/11/lib/libc/mips/sys/Makefile.inc 305329 2016-09-03 09:03:40Z kib $

SRCS+=	trivial-vdso_tc.c

MDASM=  Ovfork.S brk.S cerror.S exect.S \
	sbrk.S syscall.S

# Don't generate default code for these syscalls:
NOASM=	break.o exit.o getlogin.o openbsd_poll.o sstk.o vfork.o yield.o

PSEUDO= _exit.o _getlogin.o
