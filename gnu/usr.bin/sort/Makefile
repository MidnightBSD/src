# $FreeBSD: release/7.0.0/gnu/usr.bin/sort/Makefile 133549 2004-08-12 06:02:28Z tjr $

SORTDIR= ${.CURDIR}/../../../contrib/gnu-sort
.PATH: ${SORTDIR}/lib ${SORTDIR}/src ${SORTDIR}/man

PROG=	sort
SRCS=   sort.c \
	__fpending.c \
	argmatch.c \
	closeout.c \
	dup-safer.c \
	error.c \
	exitfail.c \
	fopen-safer.c \
	hard-locale.c \
	human.c \
	long-options.c \
	memcoll.c \
	physmem.c \
	posixver.c \
	quote.c \
	quotearg.c \
	strnlen.c \
	umaxtostr.c \
	version-etc.c \
	xalloc-die.c \
	xmalloc.c \
	xmemcoll.c \
	xstrtoul.c \
	xstrtoumax.c

CFLAGS+=-I${.CURDIR} -I${SORTDIR}/lib -DHAVE_CONFIG_H \
	-DHAVE_LANGINFO_H=1 -DHAVE_NL_LANGINFO=1

.include <bsd.prog.mk>
