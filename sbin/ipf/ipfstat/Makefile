#	$FreeBSD: release/10.0.0/sbin/ipf/ipfstat/Makefile 145524 2005-04-25 18:55:52Z darrenr $

NOGCCERROR=	# defined

PROG=		ipfstat
SRCS=		ipfstat.c
MAN=		ipfstat.8
DPADD+=  	${LIBCURSES}
LDADD+=  	-lcurses

.include <bsd.prog.mk>
