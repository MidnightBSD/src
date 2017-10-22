# $FreeBSD: release/10.0.0/usr.sbin/ckdist/Makefile 80029 2001-07-20 06:20:32Z obrien $

.PATH: ${.CURDIR}/../../usr.bin/cksum

PROG=	ckdist
SRCS=   ckdist.c crc.c

DPADD=	${LIBMD}
LDADD=	-lmd

.include <bsd.prog.mk>
