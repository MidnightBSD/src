#	$Id: Makefile,v 1.1 2007-09-28 13:47:19 laffer1 Exp $
#
# Makefile.in for the cc part of pcc.
#
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
libexecdir = ${exec_prefix}/libexec
datarootdir = ${prefix}/share
mandir = ${datarootdir}/man
#strip = yes
CC = gcc
TARGOS = midnightbsd
TARGMACH = x86
CFLAGS = -g -DLIBEXECDIR=\"${libexecdir}\" -DINCLUDEDIR=\"${includedir}\" -static-libgcc
CPPFLAGS =  -I../../os/${TARGOS} -I${MDIR} -Dmach_${TARGMACH} -Dos_${TARGOS}
LIBS = 
LDFLAGS = 
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}

OBJS=cc.o compat.o
DEST=pcc

MIPDIR=../../mip
MDIR=../../arch/$(TARGMACH)

all: ${DEST}

${DEST}: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@

.c.o:
	$(CC) $(DEFS) $(CPPFLAGS) $(CFLAGS) -c $<

compat.o: $(MIPDIR)/compat.c
	$(CC) $(DEFS) $(CPPFLAGS) $(CFLAGS) -c -o $@ $(MIPDIR)/compat.c

install:
	test -z "${DESTDIR}$(bindir)" || mkdir -p "${DESTDIR}$(bindir)"
	${INSTALL_PROGRAM} ${DEST} ${DESTDIR}${bindir}
	@if [ ${strip} = yes ]; then		\
		strip ${DESTDIR}${bindir}/${DEST} ;	\
		echo strip ${DESTDIR}${bindir}/${DEST} ;	\
	fi
	test -z "${DESTDIR}$(mandir)/man1" || mkdir -p "${DESTDIR}$(mandir)/man1"
	${INSTALL} cc.1 ${DESTDIR}${mandir}/man1/${DEST}.1

clean:
	/bin/rm -f  $(OBJS) ${DEST}

distclean: clean
	/bin/rm -f  Makefile
