# $FreeBSD: release/10.0.0/gnu/lib/libreadline/history/Makefile 211243 2010-08-12 20:46:49Z will $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp -f ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
