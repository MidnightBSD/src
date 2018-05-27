# $MidnightBSD$
# Makefile for syscall tables
#
# $FreeBSD: stable/10/sys/i386/linux/Makefile 315063 2017-03-11 08:40:59Z dchagin $

all:
	@echo "make sysent only"

sysent:  linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c

linux_sysent.c linux_syscall.h linux_proto.h linux_syscalls.c linux_systrace_args.c: \
		../../kern/makesyscalls.sh syscalls.master syscalls.conf
	sh ../../kern/makesyscalls.sh syscalls.master syscalls.conf
