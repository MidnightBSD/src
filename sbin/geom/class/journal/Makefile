# $FreeBSD: release/7.0.0/sbin/geom/class/journal/Makefile 166626 2007-02-10 17:59:46Z pjd $

.PATH:	${.CURDIR}/../../misc

CLASS=	journal
SRCS+=	geom_journal_ufs.c

DPADD=	${LIBMD} ${LIBUFS}
LDADD=	-lmd -lufs

CFLAGS+=-I${.CURDIR}/../../../../sys

.include <bsd.lib.mk>
