#	from @(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD: release/10.0.0/lib/libc/gmon/Makefile.inc 163739 2006-10-28 13:34:35Z bde $

# gmon sources
.PATH: ${.CURDIR}/gmon

SRCS+=	gmon.c mcount.c

SYM_MAPS+=${.CURDIR}/gmon/Symbol.map

MAN+=	moncontrol.3

MLINKS+=moncontrol.3 monstartup.3

# mcount cannot be compiled with profiling
mcount.po: mcount.o
	cp mcount.o mcount.po
