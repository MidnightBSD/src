# $FreeBSD: stable/11/tools/regression/sysvsem/Makefile 264400 2014-04-13 05:21:56Z imp $
# SysV Semaphores Regression Utility 
# Obtained from: $NetBSD: Makefile,v 1.3 2002/03/05 21:57:00 thorpej Exp $

PROG=	semtest
MAN=

regress:
	@if ./semtest ./semtest; then \
		echo "PASS"; \
	else \
		echo "FAIL"; \
	fi

.include <bsd.prog.mk>
