# $FreeBSD: stable/9/sbin/pflogd/Makefile 223637 2011-06-28 11:57:25Z bz $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-include ${.CURDIR}/../../lib/libpcap/config.h

LDADD=	-lpcap
DPADD=	${LIBPCAP} ${LIBUTIL}

WARNS?=	2

.include <bsd.prog.mk>
