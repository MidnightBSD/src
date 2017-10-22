#	$FreeBSD: release/10.0.0/tools/tools/kttcp/Makefile 122628 2003-11-13 21:24:09Z sam $

SHELL=	/bin/sh

PROG=	kttcp
SRCS=	kttcp.c
BINDIR?=/usr/local/bin
SYSDIR?=/usr/src/sys

CFLAGS	+= -I${SYSDIR} -Isys

all:	kttcp module

module:
	cd sys; SYSDIR=${SYSDIR} make

install:
	install kttcp ${DESTDIR}/${BINDIR}
	cd sys; SYSDIR=${SYSDIR} make install

clean:
	rm -f ${PROG}
	cd sys; SYSDIR=${SYSDIR} make clean
