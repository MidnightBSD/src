#	$Id: Makefile,v 1.1 2007-09-28 13:47:20 laffer1 Exp $
#
# Makefile.in for the cc part of pcc.
#
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libexecdir = ${exec_prefix}/libexec
CC = gcc
TARGOS = midnightbsd
TARGMACH = x86
CFLAGS = -g -DLIBEXECDIR=\"${libexecdir}\"
CPPFLAGS =  -I../os/${TARGOS} -Dmach_${TARGMACH} -Dos_${TARGOS}
LIBS = 
LDFLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}

OBJS=cc.o
DEST=pcc

all: ${DEST}

${DEST}: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(DEFS) $(CPPFLAGS) $(CFLAGS) -c $<

install:
	${INSTALL_PROGRAM} ${DEST} ${bindir}
	@if [ ${strip} = yes ]; then		\
		strip ${bindir}/${DEST} ;	\
		echo strip ${bindir}/${DEST} ;	\
	fi

clean:
	/bin/rm -f  $(OBJS) ${DEST}

distclean: clean
	/bin/rm -f  Makefile
