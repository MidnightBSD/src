# $FreeBSD: release/7.0.0/sbin/pflogd/Makefile 136902 2004-10-24 14:25:53Z ru $

.PATH:	${.CURDIR}/../../contrib/pf/pflogd

PROG=	pflogd
SRCS= 	pflogd.c pidfile.c privsep.c privsep_fdpass.c
MAN=	pflogd.8

CFLAGS+=-Wall -Werror -Wmissing-prototypes -Wshadow
LDADD=	-lpcap -lutil
DPADD=	${LIBPCAP} ${LIBUTIL}

.include <bsd.prog.mk>
