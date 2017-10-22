#	@(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: release/7.0.0/usr.sbin/timed/SMM.doc/timed/Makefile 172506 2007-10-10 16:59:15Z cvs2svn $

DIR=	smm/12.timed
SRCS=	timed.ms
MACROS=	-ms
PRINTER=Pdp

paper.${PRINTER}: ${SRCS}
	${SOELIM} ${SRCS} | ${TBL} | ${ROFF} > ${.TARGET}

.include <bsd.doc.mk>
