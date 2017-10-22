# $FreeBSD: release/7.0.0/gnu/usr.bin/gdb/gdbserver/Makefile 157574 2006-04-06 22:24:01Z jmg $

# Not elf specific so don't install in /usr/libexec/elf
BINDIR=/usr/bin

GDBDIR=	${.CURDIR}/../../../../contrib/gdb
.PATH:	${GDBDIR}/gdb/signals
.PATH:	${GDBDIR}/gdb/gdbserver
.PATH:	${GDBDIR}/gdb

PROG=	gdbserver

SRCS=	i387-fp.c inferiors.c mem-break.c regcache.c remote-utils.c	\
	server.c signals.c target.c reg-i386.c utils.c
SRCS+=	fbsd-low.c fbsd-i386-low.c

#CFLAGS+=	-I${.CURDIR}/../arch/${MACHINE_ARCH}
CFLAGS+=	-I${GDBDIR}/gdb/gdbserver
CFLAGS+=	-I${GDBDIR}/gdb/regformats
CFLAGS+=	-DNO_MMALLOC -DGDBSERVER

.include <bsd.prog.mk>
