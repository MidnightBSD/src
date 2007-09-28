#	$Id: Makefile,v 1.1 2007-09-28 13:47:19 laffer1 Exp $
#
# Makefile.in for top-level of pcc.
#


CC=gcc
CFLAGS=-g
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

${SUBDIR}: nonexistant
	cd $@; $(MAKE) all $(MFLAGS)

nonexistant:
