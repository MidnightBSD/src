# Makefile for syscall tables
#
# $FreeBSD: stable/11/sys/compat/svr4/Makefile 357108 2020-01-25 05:47:56Z kevans $

# Don't use an OBJDIR
.OBJDIR: ${.CURDIR}

MAKESYSCALLS=	../../kern/makesyscalls.sh
GENERATED=	svr4_sysent.c		\
		svr4_syscall.h		\
		svr4_syscallnames.c	\
		svr4_proto.h

all:
	@echo "make sysent only"

.ORDER: ${GENERATED}
sysent:  ${GENERATED}

${GENERATED}: ${MAKESYSCALLS} syscalls.master syscalls.conf
	sh ${MAKESYSCALLS} syscalls.master syscalls.conf

