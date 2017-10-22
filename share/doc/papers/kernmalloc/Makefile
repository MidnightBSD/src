#	From: @(#)Makefile	1.8 (Berkeley) 6/8/93
# $FreeBSD: release/7.0.0/share/doc/papers/kernmalloc/Makefile 172506 2007-10-10 16:59:15Z cvs2svn $

VOLUME=		papers
DOC=		kernmalloc
SRCS=		kernmalloc.t appendix.ms
EXTRA=		alloc.fig usage.tbl
MACROS=		-ms
USE_EQN=
USE_PIC=
USE_SOELIM=
USE_TBL=
CLEANFILES=	appendix.ms

appendix.ms: appendix.t
	${GRIND} < ${.ALLSRC} > ${.TARGET}

.include <bsd.doc.mk>
