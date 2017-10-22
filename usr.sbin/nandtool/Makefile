# $FreeBSD: release/10.0.0/usr.sbin/nandtool/Makefile 235537 2012-05-17 10:11:18Z gber $

PROG=	nandtool
SRCS=	nandtool.c nand_read.c nand_write.c nand_erase.c nand_info.c
SRCS+=	nand_readoob.c nand_writeoob.c
BINDIR=	/usr/sbin
DPADD=	${LIBGEOM}
LDADD=	-lgeom
MAN=	nandtool.8

.include <bsd.prog.mk>
