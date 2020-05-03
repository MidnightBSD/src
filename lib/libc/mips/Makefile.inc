#	$NetBSD: Makefile.inc,v 1.7 2005/09/17 11:49:39 tsutsui Exp $
# $FreeBSD: stable/11/lib/libc/mips/Makefile.inc 315044 2017-03-11 02:51:29Z brooks $

CFLAGS+=-DSOFTFLOAT

SRCS+= machdep_ldisd.c
SYM_MAPS+= ${LIBC_SRCTOP}/mips/Symbol.map
