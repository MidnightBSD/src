# $FreeBSD: stable/11/lib/libc/aarch64/sys/Makefile.inc 305330 2016-09-03 09:24:33Z kib $

MIASM:=	${MIASM:Nfreebsd[467]_*}

SRCS+=	__vdso_gettc.c

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
