#	@(#)Makefile	8.2 (Berkeley) 4/19/94

DB=	freebsd-tips startrek

BLDS=	${DB:S/$/.dat/}
FILES=	${DB} ${BLDS}
CLEANFILES+=${BLDS}

FILESDIR=	${SHAREDIR}/games/fortune

.for f in ${DB}
$f.dat: $f
	PATH=$$PATH:/usr/bin:${FORTUNE_OBJ}/strfile \
	    strfile -Cs ${.ALLSRC} ${.TARGET}
.endfor

.include <bsd.prog.mk>
