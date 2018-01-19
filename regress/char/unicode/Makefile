# $OpenBSD: Makefile,v 1.4 2014/12/19 04:57:11 schwarze Exp $

REGRESS_TARGETS  = ascii input invalid latin1 latin1diff
REGRESS_TARGETS += man mdoc named namediff nogroff
SKIP_ASCII 	 = man mdoc
# input and nogroff exhibit implementation dependent differences
# among wcwidth(3) on different systems
#UTF8_TARGETS	 = ${REGRESS_TARGETS}
UTF8_TARGETS	 = ascii invalid man mdoc namediff
HTML_TARGETS	 = ascii invalid latin1 latin1diff named namediff nogroff
LINT_TARGETS	 = input invalid

SKIP_GROFF	 = input nogroff
SKIP_GROFF_ASCII = latin1diff namediff

.include <bsd.regress.mk>
