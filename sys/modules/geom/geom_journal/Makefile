# $FreeBSD: stable/11/sys/modules/geom/geom_journal/Makefile 319182 2017-05-30 04:11:12Z ngie $

.PATH: ${SRCTOP}/sys/geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
