# $FreeBSD: release/10.0.0/sys/modules/geom/geom_journal/Makefile 163850 2006-10-31 22:21:47Z pjd $

.PATH: ${.CURDIR}/../../../geom/journal

KMOD=	geom_journal
SRCS=	g_journal.c
SRCS+=	g_journal_ufs.c
SRCS+=	vnode_if.h

.include <bsd.kmod.mk>
