# $FreeBSD: stable/11/tools/test/malloc/Makefile 264400 2014-04-13 05:21:56Z imp $
PROG=	malloc
SRCS=	main.c
.PATH:	${.CURDIR}/../../../lib/libc/stdlib

MAN=
WARNS?=6

test:	malloc
	@echo
	@csh -x -c "time ./malloc      500000  2000 8192"
	@csh -x -c "time ./malloc    50000000  2000 8192"
	@csh -x -c "time ./malloc      500000 14000 8192"
	@csh -x -c "time ./malloc    20000000 20000 2048"

.include <bsd.prog.mk>
