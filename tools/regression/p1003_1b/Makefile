# $FreeBSD: release/10.0.0/tools/regression/p1003_1b/Makefile 139103 2004-12-21 08:47:35Z ru $

PROG=p1003_1b

SRCS=\
	fifo.c \
	main.c \
	memlock.c \
	prutil.c \
	p26.c \
	sched.c \
	yield.c

NO_MAN=

CFLAGS+=-DNO_MEMLOCK
.include <bsd.prog.mk>
