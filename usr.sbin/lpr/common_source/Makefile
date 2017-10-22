# $FreeBSD: release/10.0.0/usr.sbin/lpr/common_source/Makefile 201390 2010-01-02 11:07:44Z ed $

#
# Library of internal routines for the print spooler suite.
# Originally these were compiled separately into each program,
# but the library makes it much easier to modularize them.
#
LIB=		lpr
INTERNALLIB=
SRCS=		common.c ctlinfo.c displayq.c matchjobs.c net.c \
		printcap.c request.c rmjob.c startdaemon.c

WARNS?=		1

.include <bsd.lib.mk>
