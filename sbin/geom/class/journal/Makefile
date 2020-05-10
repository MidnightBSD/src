# $FreeBSD: stable/11/sbin/geom/class/journal/Makefile 319185 2017-05-30 04:17:22Z ngie $

PACKAGE=runtime
.PATH: ${.CURDIR:H:H}/misc

GEOM_CLASS=	journal
SRCS+=	geom_journal_ufs.c

LIBADD=	ufs md

CFLAGS+=-I${SRCTOP}/sys

.include <bsd.lib.mk>
