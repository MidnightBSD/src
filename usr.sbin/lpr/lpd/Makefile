#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: release/7.0.0/usr.sbin/lpr/lpd/Makefile 98702 2002-06-23 20:57:18Z gad $

PROG=	lpd
MAN=	lpd.8
SRCS=	lpd.c printjob.c recvjob.c lpdchar.c modes.c

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
