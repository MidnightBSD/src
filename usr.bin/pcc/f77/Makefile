#	$Id: Makefile,v 1.2 2008-07-03 23:57:48 laffer1 Exp $
# $MidnightBSD$
#


CC=gcc
CFLAGS=-g -O2
LDFLAGS=
CPPFLAGS=
YACC=yacc
LEX=flex

SUBDIR=cc cpp ccom

all: ${SUBDIR}

install:
	cd cc && ${MAKE} install
	cd cpp && ${MAKE} install
	cd ccom && ${MAKE} install

clean:
	cd cc && ${MAKE} clean
	cd cpp && ${MAKE} clean
	cd ccom && ${MAKE} clean

distclean:
	cd cc && ${MAKE} distclean
	cd cpp && ${MAKE} distclean
	cd ccom && ${MAKE} distclean
	/bin/rm -rf Makefile config.log stamp-h1 config.status \
	configure.lineno config.h autom4te.cache

${SUBDIR}:
	cd $@; $(MAKE) all $(MFLAGS)
