# $FreeBSD: stable/11/sbin/mksnap_ffs/Makefile 319185 2017-05-30 04:17:22Z ngie $

.PATH:	${.CURDIR:H}/mount

PACKAGE=runtime
PROG=	mksnap_ffs
SRCS=	mksnap_ffs.c getmntopts.c
MAN=	mksnap_ffs.8

WARNS?=	2
CFLAGS+=-I${.CURDIR:H}/mount

.if defined(NOSUID)
BINMODE=554
.else
BINMODE=4554
BINOWN=	root
.endif
BINGRP=	operator

.include <bsd.prog.mk>
