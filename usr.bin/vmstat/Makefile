#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/7.0.0/usr.bin/vmstat/Makefile 148413 2005-07-25 22:35:10Z rwatson $

PROG=	vmstat
MAN=	vmstat.8
DPADD=	${LIBDEVSTAT} ${LIBKVM} ${LIBMEMSTAT}
LDADD=	-ldevstat -lkvm -lmemstat

.include <bsd.prog.mk>
