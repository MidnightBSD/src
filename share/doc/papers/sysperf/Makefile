#	From: @(#)Makefile	1.6 (Berkeley) 6/8/93
# $FreeBSD: release/7.0.0/share/doc/papers/sysperf/Makefile 172506 2007-10-10 16:59:15Z cvs2svn $

VOLUME=		papers
DOC=		sysperf
SRCS=		0.t 1.t 2.t 3.t 4.t 5.t 6.t 7.t appendix.tmp
EXTRA=		a1.t a2.t
MACROS=		-ms
USE_EQN=
USE_TBL=
CLEANFILES=	appendix.tmp

appendix.tmp: a1.t a2.t
	${GRIND} ${.CURDIR}/a1.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' > appendix.tmp
	${GRIND} -lcsh ${.CURDIR}/a2.t |  awk '/\.\(\)/{ cnt = 2 } \
	   { if (cnt) cnt -= 1; else print $$0; } ' >> appendix.tmp

.include <bsd.doc.mk>
