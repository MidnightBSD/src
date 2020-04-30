# $FreeBSD: stable/11/lib/libc/riscv/sys/Makefile.inc 302092 2016-06-22 21:11:27Z brooks $

SRCS+=	trivial-vdso_tc.c

#MDASM= ptrace.S
MDASM=	cerror.S \
	shmat.S \
	sigreturn.S \
	syscall.S \
	vfork.S

# Don't generate default code for these syscalls:
NOASM=	break.o \
	exit.o \
	getlogin.o \
	openbsd_poll.o \
	sbrk.o \
	sstk.o \
	vfork.o \
	yield.o

PSEUDO= _exit.o \
	_getlogin.o
