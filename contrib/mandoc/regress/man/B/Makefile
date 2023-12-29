# $OpenBSD: Makefile,v 1.1 2014/08/14 02:00:52 schwarze Exp $

REGRESS_TARGETS	= args blank
LINT_TARGETS	= args blank

# groff-1.22.3 defect:
# - A blank line in next line scope causes a blank line.
SKIP_GROFF	= blank

.include <bsd.regress.mk>
