# $MidnightBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.2 2007/07/26 20:27:46 laffer1 Exp $
# $FreeBSD: src/gnu/lib/libreadline/Makefile.inc,v 1.11.2.1 2005/07/22 17:29:02 kensmith Exp $

SHLIB_MAJOR=7
RL_LIBRARY_VERSION=5.2

SRCDIR= ${.CURDIR}/../../../../contrib/libreadline
.PATH: ${SRCDIR} ${.CURDIR}/..

LOCAL_CFLAGS= -DRL_LIBRARY_VERSION='"$(RL_LIBRARY_VERSION)"'
CFLAGS+=-I${.CURDIR}/.. -I${SRCDIR} -DHAVE_CONFIG_H ${LOCAL_CFLAGS}

HISTSRC=history.c histexpand.c histfile.c histsearch.c shell.c mbutil.c

.include "../Makefile.inc"
