# $FreeBSD: stable/11/usr.bin/nc/Makefile 319186 2017-05-30 04:27:05Z ngie $

.PATH: ${SRCTOP}/contrib/netcat

PROG=	nc
SRCS=	netcat.c atomicio.c socks.c

CFLAGS+=-DIPSEC
LIBADD=	ipsec

WARNS?=	2

.include <bsd.prog.mk>
