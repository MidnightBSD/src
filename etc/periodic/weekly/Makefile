# $MidnightBSD: src/etc/periodic/weekly/Makefile,v 1.4 2011/08/07 01:28:59 laffer1 Exp $

.include <bsd.own.mk>

FILES=	340.noid \
	350.msearch \
	999.local

# NB: keep these sorted by MK_* knobs

.if ${MK_LOCATE} != "no"
FILES+=	310.locate
.endif

.if ${MK_MAN_UTILS} != "no"
FILES+=	320.whatis 330.catman
.endif

.include <bsd.prog.mk>
