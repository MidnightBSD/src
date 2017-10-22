# $FreeBSD: release/7.0.0/gnu/lib/libreadline/history/Makefile 75808 2001-04-21 22:05:33Z ache $

SUBDIR= doc

LIB=    history
MAN=    rlhistory.3

SRCS= $(HISTSRC) xmalloc.c

rlhistory.3: doc/history.3
	cp ${.ALLSRC} ${.TARGET}

CLEANFILES+= rlhistory.3

.include <bsd.lib.mk>
