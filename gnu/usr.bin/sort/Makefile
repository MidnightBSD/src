# $MidnightBSD: src/gnu/usr.bin/sort/Makefile,v 1.2 2011/02/06 00:52:01 laffer1 Exp $
# $FreeBSD: src/gnu/usr.bin/sort/Makefile,v 1.12 2004/08/12 06:02:28 tjr Exp $

SORTDIR= ${.CURDIR}/../../../contrib/gnu-sort
.PATH: ${SORTDIR}/lib ${SORTDIR}/src ${SORTDIR}/man

PROG=	gnusort
SRCS=   sort.c \
	__fpending.c \
	argmatch.c \
	closeout.c \
	close-stream.c \
	dup-safer.c \
	error.c \
	exitfail.c \
	fd-safer.c \
	fopen-safer.c \
	gethrxtime.c \
	gettime.c \
	hard-locale.c \
	hash.c \
	human.c \
	long-options.c \
	md5.c \
	memcoll.c \
	memxfrm.c \
	mkstemp-safer.c \
	physmem.c \
	posixver.c \
	quote.c \
	quotearg.c \
	rand-isaac.c \
	randread.c \
	strnlen.c \
	strnumcmp.c \
	umaxtostr.c \
	version-etc.c \
	version-etc-fsf.c \
	xalloc-die.c \
	xmalloc.c \
	xmemcoll.c \
	xmemxfrm.c \
	xstrtoul.c \
	xstrtoumax.c

MAN1+=	sort.1
CFLAGS+=-I${.CURDIR} -I${SORTDIR}/lib -DHAVE_CONFIG_H \
	-DHAVE_LANGINFO_H=1 -DHAVE_NL_LANGINFO=1 \
	-std=c99

.include <bsd.prog.mk>
