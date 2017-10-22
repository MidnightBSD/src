# $FreeBSD: release/10.0.0/sbin/nandfs/Makefile 235537 2012-05-17 10:11:18Z gber $

PROG=	nandfs
SRCS=	nandfs.c lssnap.c mksnap.c rmsnap.c
MAN=	nandfs.8

DPADD=	${LIBNANDFS}
LDADD=	-lnandfs

.include <bsd.prog.mk>
