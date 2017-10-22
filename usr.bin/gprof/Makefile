#	@(#)Makefile	8.1 (Berkeley) 6/29/93
#	$FreeBSD: release/10.0.0/usr.bin/gprof/Makefile 201386 2010-01-02 10:27:05Z ed $

PROG=	gprof
SRCS=	gprof.c aout.c arcs.c dfn.c elf.c lookup.c hertz.c \
	printgprof.c printlist.c kernel.c
FILES=	gprof.flat gprof.callg
FILESDIR=	${SHAREDIR}/misc

WARNS?=	1

.include <bsd.prog.mk>
