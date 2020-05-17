#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: stable/11/usr.bin/lastcomm/Makefile 298107 2016-04-16 07:45:30Z gjb $

.include <src.opts.mk>

PACKAGE=acct

PROG=	lastcomm
SRCS=	lastcomm.c readrec.c

.if ${MK_TESTS} != "no"
SUBDIR+= tests
.endif

.include <bsd.prog.mk>
