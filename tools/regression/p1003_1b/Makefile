# $FreeBSD: stable/11/tools/regression/p1003_1b/Makefile 282072 2015-04-27 08:34:46Z ngie $

PROG=p1003_1b

SRCS=\
	fifo.c \
	main.c \
	memlock.c \
	prutil.c \
	p26.c \
	sched.c \
	yield.c

MAN=

CFLAGS+=-DNO_MEMLOCK

.include <bsd.prog.mk>
