#
# $FreeBSD: release/7.0.0/usr.sbin/pmcstat/Makefile 172401 2007-10-01 18:15:11Z ru $
#

PROG=	pmcstat
MAN=	pmcstat.8

DPADD=	${LIBKVM} ${LIBPMC} ${LIBM}
LDADD=	-lkvm -lpmc -lm

WARNS?=	6

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c

.include <bsd.prog.mk>
