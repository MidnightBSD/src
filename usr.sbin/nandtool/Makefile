# $FreeBSD: stable/11/usr.sbin/nandtool/Makefile 275054 2014-11-25 16:57:27Z bapt $

PROG=	nandtool
SRCS=	nandtool.c nand_read.c nand_write.c nand_erase.c nand_info.c
SRCS+=	nand_readoob.c nand_writeoob.c
BINDIR=	/usr/sbin
LIBADD=	geom
MAN=	nandtool.8

.include <bsd.prog.mk>
