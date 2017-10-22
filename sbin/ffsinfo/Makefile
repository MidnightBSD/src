# @(#)Makefile    8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/ffsinfo/Makefile,v 1.3 2000/12/05 19:45:10 tomsoft Exp $
# $FreeBSD: release/7.0.0/sbin/ffsinfo/Makefile 127729 2004-04-01 20:31:49Z johan $
#

GROWFS= ${.CURDIR}/../growfs
.PATH: ${GROWFS}

PROG=	ffsinfo
SRCS=	ffsinfo.c debug.c
MAN=	ffsinfo.8

WARNS?=	0
CFLAGS+=-DFS_DEBUG -I${GROWFS}
DPADD=	${LIBUFS}
LDADD=	-lufs
 
.include <bsd.prog.mk>               
