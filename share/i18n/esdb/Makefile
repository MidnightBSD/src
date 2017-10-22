# $FreeBSD: stable/9/share/i18n/esdb/Makefile 219019 2011-02-25 00:04:39Z gabor $
# $NetBSD: Makefile,v 1.14 2007/04/01 18:52:30 tnozaki Exp $

FILESDIR= ${ESDBDIR}

SUBDIR=	APPLE AST BIG5 CP DEC EUC EBCDIC GB GEORGIAN ISO-2022 ISO-8859 \
	ISO646 KAZAKH KOI MISC TCVN UTF

FILES+=	esdb.dir esdb.dir.db esdb.alias esdb.alias.db
CLEANFILES= ${FILES}

esdb.dir: ${SUBDIR}
	touch $@
.for i in ${SUBDIR}
	cat ${i}/esdb.dir.${i} >>${.TARGET}
.endfor
esdb.dir.db: esdb.dir
	${MKESDB} -m -o ${.TARGET} ${.ALLSRC}

esdb.alias: ${SUBDIR}
	touch $@
.for i in ${SUBDIR}
	cat ${i}/esdb.alias.${i} >>${.TARGET}
.endfor
esdb.alias.db: esdb.alias
	${MKESDB} -m -o ${.TARGET} ${.ALLSRC}

all: ${FILES}
realall: ${FILES}

.include "./Makefile.inc"
.include <bsd.prog.mk>
