# $FreeBSD: release/10.0.0/gnu/lib/libregex/Makefile 251438 2013-06-05 21:31:06Z jkim $

SUBDIR=		doc

LIB=		gnuregex
SHLIB_MAJOR=	5

REGEXDIR=	${.CURDIR}/../../../contrib/libgnuregex
.PATH:		${REGEXDIR}

SRCS=		gnuregex.c
INCSGROUPS=	INCS WRINCS PXINCS
INCS=		regex.h.patched
INCSNAME=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=		gnuregex.h
PXINCS=		${REGEXDIR}/regex.h
PXINCSDIR=	${INCSDIR}/posix

CFLAGS+=	-D__attribute_warn_unused_result__=""
CFLAGS+=	-DHAVE_CONFIG_H -I${.CURDIR} -I${REGEXDIR}

CLEANFILES=	regex.h.patched gnuregex.c
regex.h.patched: regex.h
	sed 's=<posix/regex\.h>=<gnu/posix/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

gnuregex.c: regex.c
	sed 's=<regex\.h>=<gnu/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
