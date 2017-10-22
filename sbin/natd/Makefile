# $FreeBSD: release/7.0.0/sbin/natd/Makefile 126178 2004-02-23 20:25:27Z johan $

PROG		= natd
SRCS		= natd.c icmp.c
WARNS?=	0
LDADD		= -lalias
DPADD		= ${LIBALIAS}
MAN		= natd.8

.include <bsd.prog.mk>
