# $FreeBSD: release/7.0.0/tools/tools/ath/athstats/Makefile 166408 2007-02-02 02:39:56Z sam $

PROG=	athstats
BINDIR=	/usr/local/bin
NO_MAN=

SRCS=	main.c statfoo.c athstats.c

.include <bsd.prog.mk>

CFLAGS+= -I.
CLEANFILES+= opt_ah.h

athstats.o: opt_ah.h

opt_ah.h:
	touch opt_ah.h
