#
# $FreeBSD: release/10.0.0/tools/regression/pipe/Makefile 139103 2004-12-21 08:47:35Z ru $
#
# "make" then "make regress".
#
PROG=	bigpipetest
NO_MAN=

regress:
	@if ./bigpipetest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
