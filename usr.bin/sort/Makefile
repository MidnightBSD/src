# $MidnightBSD$
# $FreeBSD: stable/10/usr.bin/sort/Makefile 281834 2015-04-21 15:47:00Z garga $

.include <bsd.own.mk>

PROG=	sort

SRCS=	bwstring.c coll.c file.c mem.c radixsort.c sort.c vsort.c

sort.1: sort.1.in
	/usr/bin/sed ${MAN_SUB} ${.ALLSRC} >${.TARGET}

CLEANFILES+= sort.1

.if defined(WITH_THREADS)
CFLAGS+= -DSORT_THREADS
LDADD+= -lpthread -lmd
MAN_SUB+= -e 's|%%THREADS%%||g'
.else
LDADD+= -lmd
MAN_SUB+= -e 's|%%THREADS%%|\.\\"|g'
.endif

CFLAGS+= -DWITHOUT_NLS
MAN_SUB+= -e 's|%%NLS%%|\.\\"|g'

.include <bsd.prog.mk>
